#include "src/devices/m5stack_tab5/device_m5stack_tab5.h"

#include "src/devices/device_select.h"

#if defined(DEVICE_M5STACK_TAB5)

#include <Arduino_GFX_Library.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include "esp_cache.h"
#include <esp_err.h>
#include <esp_ldo_regulator.h>

#include "src/devices/m5stack_tab5/vendor/display_config.h"

namespace {

constexpr int kSdClkPin = 43;
constexpr int kSdMisoPin = 39;
constexpr int kSdMosiPin = 44;
constexpr int kSdCsPin = 42;
constexpr uint32_t kSdClockHz = 25000000;

constexpr int kMipiLdoChannel = 3;
constexpr int kMipiLdoVoltageMv = 2500;

constexpr gpio_num_t kBacklightPin = GPIO_NUM_22;
constexpr ledc_channel_t kBacklightChannel = LEDC_CHANNEL_1;
constexpr ledc_timer_t kBacklightTimer = LEDC_TIMER_0;
constexpr uint32_t kBacklightFreq = 44100;
constexpr ledc_timer_bit_t kBacklightBits = LEDC_TIMER_10_BIT;
constexpr bool kBacklightActiveLow = false;

constexpr uint8_t kTouchAddr = 0x55;
constexpr int kTouchSdaPin = 31;
constexpr int kTouchSclPin = 32;
constexpr int kTouchIntPin = 23;
constexpr uint32_t kTouchClockHz = 400000;
constexpr uint32_t kPi4ioClockHz = 100000;
constexpr uint8_t kPi4io1Addr = 0x43;
constexpr uint8_t kPi4io2Addr = 0x44;
constexpr uint16_t kTouchMaxRawX = 719;
constexpr uint16_t kTouchMaxRawY = 1279;
constexpr uint16_t kTouchRegFwVersion = 0x0000;
constexpr uint16_t kTouchRegFwRevision = 0x000C;
constexpr uint16_t kTouchRegMaxX = 0x0005;
constexpr uint16_t kTouchRegMaxTouches = 0x0009;
constexpr uint16_t kTouchRegAdvInfo = 0x0010;
constexpr uint16_t kTouchRegCoords = 0x0014;

Arduino_ESP32DSIPanel* g_dsi_panel = nullptr;
Arduino_DSI_Display* g_gfx = nullptr;
TwoWire g_touch_wire(1);

bool g_display_ready = false;
bool g_touch_ready = false;
bool g_backlight_ready = false;
bool g_pi4io_ready = false;
bool g_sd_init_attempted = false;
bool g_sd_available = false;
uint32_t g_sd_retry_tick_ms = 0;
uint8_t g_brightness = 150;
uint8_t g_rotation = DeviceM5StackTab5::kProfile.rotation_default;

uint8_t to_panel_rotation(uint8_t logical_rotation) {
  logical_rotation &= 0x03;
  return (logical_rotation & 0x02) ? 3 : 1;
}

void panel_flush() {
  if (g_gfx) {
    g_gfx->flush();
  }
}

void apply_backlight(uint8_t value) {
  if (!g_backlight_ready) {
    return;
  }

  constexpr uint32_t kMaxDuty = (1u << 10) - 1u;
  uint32_t duty = ((uint32_t)value * kMaxDuty + 127u) / 255u;
  if (kBacklightActiveLow) {
    duty = kMaxDuty - duty;
  }

  ledc_set_duty(LEDC_LOW_SPEED_MODE, kBacklightChannel, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, kBacklightChannel);
}

bool init_backlight() {
  if (g_backlight_ready) {
    return true;
  }

  Serial.println("[Device/M5StackTab5] Backlight: timer setup...");
  Serial.flush();
  ledc_timer_config_t timer_cfg = {};
  timer_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
  timer_cfg.timer_num = kBacklightTimer;
  timer_cfg.duty_resolution = kBacklightBits;
  timer_cfg.freq_hz = kBacklightFreq;
  timer_cfg.clk_cfg = LEDC_AUTO_CLK;
  if (ledc_timer_config(&timer_cfg) != ESP_OK) {
    Serial.println("[Device/M5StackTab5] Backlight timer init failed");
    return false;
  }

  Serial.println("[Device/M5StackTab5] Backlight: channel setup...");
  Serial.flush();
  ledc_channel_config_t ch_cfg = {};
  ch_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
  ch_cfg.channel = kBacklightChannel;
  ch_cfg.timer_sel = kBacklightTimer;
  ch_cfg.gpio_num = kBacklightPin;
  ch_cfg.duty = 0;
  ch_cfg.hpoint = 0;
  if (ledc_channel_config(&ch_cfg) != ESP_OK) {
    Serial.println("[Device/M5StackTab5] Backlight channel init failed");
    return false;
  }

  g_backlight_ready = true;
  apply_backlight(0);
  Serial.println("[Device/M5StackTab5] Backlight: ready");
  Serial.flush();
  return true;
}

bool i2c_write_reg8(TwoWire& wire, uint8_t addr, uint8_t reg, uint8_t value) {
  wire.beginTransmission(addr);
  wire.write(reg);
  wire.write(value);
  return wire.endTransmission() == 0;
}

bool pi4io_write_triplets(TwoWire& wire, uint8_t addr, const uint8_t* triplets) {
  for (size_t i = 0;; i += 3) {
    const uint8_t reg = triplets[i + 0];
    const uint8_t value = triplets[i + 1];
    const uint8_t delay_ms = triplets[i + 2];
    if (reg == 0xFF && value == 0xFF && delay_ms == 0xFF) {
      return true;
    }
    if (!i2c_write_reg8(wire, addr, reg, value)) {
      return false;
    }
    if (delay_ms) {
      delay(delay_ms);
    }
  }
}

bool init_pi4io_reset() {
  if (g_pi4io_ready) {
    return true;
  }

  static constexpr uint8_t kPi4io1Low[] = {
      0x03, 0b01111111, 0,
      0x05, 0b01000110, 0,
      0x07, 0b00000000, 0,
      0x0D, 0b01111111, 0,
      0x0B, 0b01111111, 0,
      0xFF, 0xFF, 0xFF,
  };
  static constexpr uint8_t kPi4io1High[] = {
      0x05, 0b01110110, 0,
      0xFF, 0xFF, 0xFF,
  };
  static constexpr uint8_t kPi4io2Init[] = {
      0x03, 0b10111001, 0,
      0x07, 0b00000110, 0,
      0x0D, 0b10111001, 0,
      0x0B, 0b11111001, 0,
      0x09, 0b01000000, 0,
      0x11, 0b10111111, 0,
      0x05, 0b10001001, 0,
      0xFF, 0xFF, 0xFF,
  };

  Serial.println("[Device/M5StackTab5] PI4IO: init...");
  Serial.flush();

  pinMode(kTouchIntPin, OUTPUT);
  digitalWrite(kTouchIntPin, HIGH);
  g_touch_wire.begin(kTouchSdaPin, kTouchSclPin, kPi4ioClockHz);

  g_touch_wire.beginTransmission(kPi4io1Addr);
  if (g_touch_wire.endTransmission() != 0) {
    Serial.println("[Device/M5StackTab5] PI4IO1 not responding");
    Serial.flush();
    return false;
  }
  g_touch_wire.beginTransmission(kPi4io2Addr);
  if (g_touch_wire.endTransmission() != 0) {
    Serial.println("[Device/M5StackTab5] PI4IO2 not responding");
    Serial.flush();
    return false;
  }

  if (!pi4io_write_triplets(g_touch_wire, kPi4io1Addr, kPi4io1Low) ||
      !pi4io_write_triplets(g_touch_wire, kPi4io2Addr, kPi4io2Init)) {
    Serial.println("[Device/M5StackTab5] PI4IO setup failed");
    Serial.flush();
    return false;
  }
  delay(10);
  if (!pi4io_write_triplets(g_touch_wire, kPi4io1Addr, kPi4io1High)) {
    Serial.println("[Device/M5StackTab5] PI4IO reset release failed");
    Serial.flush();
    return false;
  }

  delay(80);
  g_pi4io_ready = true;
  Serial.println("[Device/M5StackTab5] PI4IO: ready");
  Serial.flush();
  return true;
}

bool init_display() {
  if (g_display_ready) {
    return true;
  }

  // LDO for MIPI DSI PHY
  esp_ldo_channel_handle_t ldo_handle = nullptr;
  esp_ldo_channel_config_t ldo_cfg = {
      .chan_id = kMipiLdoChannel,
      .voltage_mv = kMipiLdoVoltageMv,
  };
  esp_err_t ldo_err = esp_ldo_acquire_channel(&ldo_cfg, &ldo_handle);
  if (ldo_err != ESP_OK) {
    Serial.printf("[Device/M5StackTab5] LDO acquire failed: %s\n",
                  esp_err_to_name(ldo_err));
    Serial.flush();
    return false;
  }

  Serial.println("[Device/M5StackTab5] Display: create DSI panel...");
  Serial.flush();
  g_dsi_panel = new Arduino_ESP32DSIPanel(
      kTab5DisplayConfig.hsync_pulse_width,
      kTab5DisplayConfig.hsync_back_porch,
      kTab5DisplayConfig.hsync_front_porch,
      kTab5DisplayConfig.vsync_pulse_width,
      kTab5DisplayConfig.vsync_back_porch,
      kTab5DisplayConfig.vsync_front_porch,
      kTab5DisplayConfig.prefer_speed,
      kTab5DisplayConfig.lane_bit_rate);
  if (!g_dsi_panel) {
    Serial.println("[Device/M5StackTab5] DSI panel allocation failed");
    return false;
  }

  Serial.println("[Device/M5StackTab5] Display: create display object...");
  Serial.flush();
  g_gfx = new Arduino_DSI_Display(
      kTab5DisplayConfig.width,
      kTab5DisplayConfig.height,
      g_dsi_panel,
      kTab5DisplayConfig.rotation,
      kTab5DisplayConfig.auto_flush,
      GFX_NOT_DEFINED,
      kTab5DisplayConfig.init_cmds,
      kTab5DisplayConfig.init_cmds_size);
  if (!g_gfx) {
    Serial.println("[Device/M5StackTab5] Display allocation failed");
    return false;
  }

  Serial.println("[Device/M5StackTab5] Display: begin()...");
  Serial.flush();
  if (!g_gfx->begin()) {
    Serial.println("[Device/M5StackTab5] gfx->begin() failed");
    Serial.flush();
    return false;
  }

  Serial.println("[Device/M5StackTab5] Display: apply rotation...");
  Serial.flush();
  g_gfx->setRotation(to_panel_rotation(g_rotation));
  Serial.println("[Device/M5StackTab5] Display: displayOn()...");
  Serial.flush();
  g_gfx->displayOn();
  Serial.println("[Device/M5StackTab5] Display: clear...");
  Serial.flush();
  g_gfx->fillScreen(0x0000);
  Serial.println("[Device/M5StackTab5] Display: flush...");
  Serial.flush();
  panel_flush();

  g_display_ready = true;
  Serial.println("[Device/M5StackTab5] Display: ready");
  Serial.flush();
  return true;
}

bool touch_read_regs(uint16_t reg, uint8_t* dst, size_t len) {
  if (!dst || len == 0) {
    return false;
  }

  g_touch_wire.beginTransmission(kTouchAddr);
  g_touch_wire.write((uint8_t)(reg >> 8));
  g_touch_wire.write((uint8_t)(reg & 0xFF));
  if (g_touch_wire.endTransmission(false) != 0) {
    return false;
  }

  const size_t got = g_touch_wire.requestFrom((int)kTouchAddr, (int)len);
  if (got != len) {
    while (g_touch_wire.available()) {
      g_touch_wire.read();
    }
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    dst[i] = (uint8_t)g_touch_wire.read();
  }
  return true;
}

bool init_touch() {
  if (g_touch_ready) {
    return true;
  }

  Serial.println("[Device/M5StackTab5] Touch: wire begin...");
  Serial.flush();
  pinMode(kTouchIntPin, INPUT);
  g_touch_wire.begin(kTouchSdaPin, kTouchSclPin, kTouchClockHz);

  for (int retry = 0; retry < 6; ++retry) {
    uint8_t version = 0;
    uint8_t revision[4] = {0};
    uint8_t info[5] = {0};
    if (touch_read_regs(kTouchRegFwVersion, &version, 1) &&
        touch_read_regs(kTouchRegFwRevision, revision, sizeof(revision)) &&
        touch_read_regs(kTouchRegMaxX, info, sizeof(info))) {
      uint32_t sum = version;
      for (size_t i = 0; i < sizeof(revision); ++i) {
        sum += revision[i];
      }
      for (size_t i = 0; i < sizeof(info); ++i) {
        sum += info[i];
      }
      if (sum != 0) {
        g_touch_ready = true;
        Serial.println("[Device/M5StackTab5] Touch: detected");
        Serial.flush();
        return true;
      }
    }
    delay(20);
  }

  Serial.println("[Device/M5StackTab5] Touch init failed");
  Serial.flush();
  return false;
}

bool read_touch_raw(uint16_t& raw_x, uint16_t& raw_y, uint8_t& area) {
  uint8_t adv_info = 0;
  if (!touch_read_regs(kTouchRegAdvInfo, &adv_info, 1)) {
    return false;
  }

  if ((adv_info & 0x08) == 0) {
    return false;
  }

  uint8_t max_touches = 0;
  if (!touch_read_regs(kTouchRegMaxTouches, &max_touches, 1)) {
    return false;
  }
  if (max_touches == 0) {
    return false;
  }
  if (max_touches > 10) {
    max_touches = 10;
  }

  uint8_t report[70] = {0};
  const size_t report_len = (size_t)max_touches * 7u;
  if (!touch_read_regs(kTouchRegCoords, report, report_len)) {
    return false;
  }

  for (uint8_t i = 0; i < max_touches; ++i) {
    const uint8_t* p = &report[i * 7u];
    if ((p[0] & 0x80u) == 0) {
      continue;
    }
    raw_x = (uint16_t)((p[0] & 0x3Fu) << 8) | p[1];
    raw_y = (uint16_t)(p[2] << 8) | p[3];
    area = p[4];
    return true;
  }

  return false;
}

void map_touch_to_screen(uint16_t raw_x, uint16_t raw_y, int16_t& x, int16_t& y) {
  int32_t mapped_x = 0;
  int32_t mapped_y = 0;

  if ((g_rotation & 0x02) != 0) {
    mapped_x = (int32_t)kTouchMaxRawY - (int32_t)raw_y;
    mapped_y = raw_x;
  } else {
    mapped_x = raw_y;
    mapped_y = (int32_t)kTouchMaxRawX - (int32_t)raw_x;
  }

  if (mapped_x < 0) mapped_x = 0;
  if (mapped_y < 0) mapped_y = 0;
  if (mapped_x >= DeviceM5StackTab5::kProfile.screen_width) {
    mapped_x = DeviceM5StackTab5::kProfile.screen_width - 1;
  }
  if (mapped_y >= DeviceM5StackTab5::kProfile.screen_height) {
    mapped_y = DeviceM5StackTab5::kProfile.screen_height - 1;
  }

  x = (int16_t)mapped_x;
  y = (int16_t)mapped_y;
}

}  // namespace

bool DeviceM5StackTab5::init() {
  if (g_display_ready && g_touch_ready) {
    return true;
  }

  Serial.println("[Device/M5StackTab5] Initialising board...");
  Serial.flush();

  if (!init_backlight()) {
    return false;
  }
  apply_backlight(g_brightness);
  Serial.printf("[Device/M5StackTab5] Backlight: preview on (%u)\n",
                static_cast<unsigned>(g_brightness));
  Serial.flush();

  if (!init_pi4io_reset()) {
    return false;
  }

  if (!init_display()) {
    return false;
  }
  Serial.println("[Device/M5StackTab5] Display OK");
  Serial.flush();

  if (!init_touch()) {
    return false;
  }
  Serial.println("[Device/M5StackTab5] Touch OK");
  Serial.flush();

  setBrightness(g_brightness);
  Serial.println("[Device/M5StackTab5] Init complete");
  Serial.flush();
  return true;
}

void DeviceM5StackTab5::displayPushPixels(int32_t x, int32_t y, int32_t w, int32_t h,
                                          const uint16_t* data) {
  if (!g_display_ready || !g_gfx || !data || w <= 0 || h <= 0) {
    return;
  }

  uint16_t* fb = g_gfx->getFramebuffer();
  if (!fb) {
    g_gfx->draw16bitRGBBitmap(x, y, const_cast<uint16_t*>(data), w, h);
    panel_flush();
    return;
  }

  // Native framebuffer is 720 wide x 1280 tall (portrait).
  // We write directly into it with rotation, iterating so that
  // the inner loop touches consecutive framebuffer addresses.
  constexpr int32_t fb_w = 720;
  constexpr int32_t fb_h = 1280;
  const uint8_t rot = to_panel_rotation(g_rotation);

  int32_t sync_first_row = 0;
  int32_t sync_last_row = 0;

  if (rot == 1) {
    // 90 deg CW: logical (lx, ly) -> fb[lx * fb_w + (fb_w-1-ly)]
    // Outer loop over logical columns -> each fixes one native row.
    // Inner loop walks consecutive (descending) native columns.
    sync_first_row = x;
    sync_last_row = x + w - 1;
    for (int32_t col = 0; col < w; ++col) {
      const int32_t nat_row = x + col;
      if (nat_row < 0 || nat_row >= fb_h) continue;
      uint16_t* dst = fb + nat_row * fb_w + (fb_w - 1 - y);
      const uint16_t* src = data + col;
      for (int32_t row = 0; row < h; ++row) {
        *dst-- = *src;
        src += w;
      }
    }
  } else {
    // 270 deg (90 CCW): logical (lx, ly) -> fb[(fb_h-1-lx) * fb_w + ly]
    // Outer loop over logical columns -> each fixes one native row.
    // Inner loop walks consecutive (ascending) native columns.
    sync_first_row = fb_h - (x + w);
    sync_last_row = fb_h - 1 - x;
    for (int32_t col = 0; col < w; ++col) {
      const int32_t nat_row = fb_h - 1 - (x + col);
      if (nat_row < 0 || nat_row >= fb_h) continue;
      uint16_t* dst = fb + nat_row * fb_w + y;
      const uint16_t* src = data + col;
      for (int32_t row = 0; row < h; ++row) {
        *dst++ = *src;
        src += w;
      }
    }
  }

  // Sync only the affected native rows to DMA memory.
  if (sync_first_row < 0) sync_first_row = 0;
  if (sync_last_row >= fb_h) sync_last_row = fb_h - 1;
  if (sync_first_row <= sync_last_row) {
    esp_cache_msync(
        fb + sync_first_row * fb_w,
        (size_t)(sync_last_row - sync_first_row + 1) * fb_w * sizeof(uint16_t),
        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
  }
}

void DeviceM5StackTab5::displayPushPixelsDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                                             const uint16_t* data) {
  displayPushPixels(x, y, w, h, data);
}

void DeviceM5StackTab5::displayWaitDMA() {
}

void DeviceM5StackTab5::displayFillScreen(uint16_t color) {
  if (!g_display_ready || !g_gfx) {
    return;
  }

  g_gfx->fillScreen(color);
  panel_flush();
}

void DeviceM5StackTab5::displaySetRotation(uint8_t rotation) {
  if (!g_display_ready || !g_gfx) {
    return;
  }

  g_rotation = rotation & 0x03;
  g_gfx->setRotation(to_panel_rotation(g_rotation));
  panel_flush();
}

void DeviceM5StackTab5::setBrightness(uint8_t value) {
  g_brightness = value;
  apply_backlight(value);
}

uint8_t DeviceM5StackTab5::getBrightness() {
  return g_brightness;
}

bool DeviceM5StackTab5::getTouch(int16_t& x, int16_t& y) {
  if (!g_touch_ready) {
    return false;
  }

  uint16_t raw_x = 0;
  uint16_t raw_y = 0;
  uint8_t area = 0;
  if (!read_touch_raw(raw_x, raw_y, area)) {
    return false;
  }

  (void)area;
  map_touch_to_screen(raw_x, raw_y, x, y);
  return true;
}

void DeviceM5StackTab5::displaySleep() {
  if (!g_display_ready || !g_gfx) {
    return;
  }

  apply_backlight(0);
  g_gfx->displayOff();
  g_gfx->flush();
}

void DeviceM5StackTab5::displayWake() {
  if (!g_display_ready || !g_gfx) {
    return;
  }

  g_gfx->displayOn();
  g_gfx->flush();
  apply_backlight(g_brightness);
}

void DeviceM5StackTab5::displayPowerSaveOn() {
  if (!g_display_ready || !g_gfx) {
    return;
  }

  apply_backlight(0);
}

void DeviceM5StackTab5::displayPowerSaveOff() {
  if (!g_display_ready || !g_gfx) {
    return;
  }

  apply_backlight(g_brightness);
}

void DeviceM5StackTab5::displayWaitDisplay() {
  if (!g_display_ready || !g_gfx) {
    return;
  }

  panel_flush();
}

bool DeviceM5StackTab5::initSDCard() {
  const uint8_t card_type = SD.cardType();
  if (g_sd_available && card_type != CARD_NONE) {
    return true;
  }

  const uint32_t now = millis();
  if (g_sd_init_attempted && (now - g_sd_retry_tick_ms) < 1500) {
    return false;
  }
  g_sd_init_attempted = true;
  g_sd_retry_tick_ms = now;

  SD.end();
  SPI.begin(kSdClkPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
  if (!SD.begin(kSdCsPin, SPI, kSdClockHz)) {
    g_sd_available = false;
    Serial.println("[Device/M5StackTab5] SD card mount failed");
    return false;
  }

  if (SD.cardType() == CARD_NONE) {
    g_sd_available = false;
    Serial.println("[Device/M5StackTab5] SD card absent after mount");
    SD.end();
    return false;
  }

  g_sd_available = true;
  Serial.printf("[Device/M5StackTab5] SD card OK, type=%u, size=%llu MB\n",
                static_cast<unsigned>(SD.cardType()),
                static_cast<unsigned long long>(SD.cardSize() / (1024ULL * 1024ULL)));
  return true;
}

bool DeviceM5StackTab5::storageReady() {
  return initSDCard();
}

fs::FS& DeviceM5StackTab5::storageFS() {
  return SD;
}

#endif

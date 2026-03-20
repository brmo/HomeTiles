#include "src/core/board_hal.h"

#include <Arduino_GFX_Library.h>
#include "src/core/waveshare_sdmmc.h"
#include <driver/gpio.h>
#include <driver/ledc.h>

#include "src/waveshare/displays_config.h"
#include "src/waveshare/gt911.h"

namespace {

Arduino_ESP32DSIPanel* g_dsi_panel = nullptr;
Arduino_DSI_Display* g_gfx = nullptr;
esp_lcd_touch_handle_t g_touch = nullptr;

uint8_t g_brightness = 150;
bool g_backlight_ready = false;
bool g_sd_init_attempted = false;
bool g_sd_available = false;
static uint32_t g_sd_retry_tick_ms = 0;

static constexpr bool kBacklightActiveLow = true;

void apply_backlight(uint8_t value) {
  if (!g_backlight_ready) {
    return;
  }

  const uint32_t max_duty = (1u << BSP_BL_LEDC_BITS) - 1u;
  uint32_t duty = ((uint32_t)value * max_duty + 127u) / 255u;
  if (kBacklightActiveLow) {
    duty = max_duty - duty;
  }

  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)BSP_BL_LEDC_CHANNEL, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)BSP_BL_LEDC_CHANNEL);
}

bool init_backlight() {
  if (g_backlight_ready) {
    return true;
  }

  ledc_timer_config_t timer_cfg = {};
  timer_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
  timer_cfg.timer_num = (ledc_timer_t)BSP_BL_LEDC_TIMER;
  timer_cfg.duty_resolution = (ledc_timer_bit_t)BSP_BL_LEDC_BITS;
  timer_cfg.freq_hz = BSP_BL_LEDC_FREQ;
  timer_cfg.clk_cfg = LEDC_AUTO_CLK;
  if (ledc_timer_config(&timer_cfg) != ESP_OK) {
    Serial.println("[HAL] Backlight timer init failed");
    return false;
  }

  ledc_channel_config_t ch_cfg = {};
  ch_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
  ch_cfg.channel = (ledc_channel_t)BSP_BL_LEDC_CHANNEL;
  ch_cfg.timer_sel = (ledc_timer_t)BSP_BL_LEDC_TIMER;
  ch_cfg.gpio_num = BSP_LCD_BACKLIGHT;
  ch_cfg.duty = kBacklightActiveLow ? ((1u << BSP_BL_LEDC_BITS) - 1u) : 0;
  ch_cfg.hpoint = 0;
  if (ledc_channel_config(&ch_cfg) != ESP_OK) {
    Serial.println("[HAL] Backlight channel init failed");
    return false;
  }

  g_backlight_ready = true;
  apply_backlight(0);
  return true;
}

bool init_display() {
  if (g_gfx) {
    return true;
  }

  g_dsi_panel = new Arduino_ESP32DSIPanel(
      display_cfg.hsync_pulse_width,
      display_cfg.hsync_back_porch,
      display_cfg.hsync_front_porch,
      display_cfg.vsync_pulse_width,
      display_cfg.vsync_back_porch,
      display_cfg.vsync_front_porch,
      display_cfg.prefer_speed,
      display_cfg.lane_bit_rate);
  if (!g_dsi_panel) {
    Serial.println("[HAL] DSI panel allocation failed");
    return false;
  }

  g_gfx = new Arduino_DSI_Display(
      display_cfg.width,
      display_cfg.height,
      g_dsi_panel,
      display_cfg.rotation,
      display_cfg.auto_flush,
      display_cfg.lcd_rst,
      display_cfg.init_cmds,
      display_cfg.init_cmds_size);
  if (!g_gfx) {
    Serial.println("[HAL] Display allocation failed");
    return false;
  }

  if (!g_gfx->begin()) {
    Serial.println("[HAL] gfx->begin() failed");
    return false;
  }

  g_gfx->displayOn();
  g_gfx->fillScreen(0x0000);
  g_gfx->flush();
  return true;
}

bool init_touch() {
  if (g_touch) {
    return true;
  }

  DEV_I2C_Port port = DEV_I2C_Init();
  g_touch = touch_gt911_init(port);
  if (!g_touch) {
    Serial.println("[HAL] Touch init failed");
    return false;
  }

  return true;
}

}  // namespace

bool BoardHAL::init() {
  Serial.println("[HAL] Initialising Waveshare board...");

  if (!init_backlight()) {
    return false;
  }
  apply_backlight(0);

  if (!init_touch()) {
    return false;
  }
  Serial.println("[HAL] Touch OK");

  if (!init_display()) {
    return false;
  }
  Serial.println("[HAL] Display OK");

  setBrightness(g_brightness);
  Serial.println("[HAL] Init complete");
  return true;
}

void BoardHAL::displayPushPixels(int32_t x, int32_t y, int32_t w, int32_t h,
                                 const uint16_t* data) {
  if (!g_gfx || !data) {
    return;
  }

  g_gfx->draw16bitRGBBitmap((int16_t)x, (int16_t)y,
                            const_cast<uint16_t*>(data),
                            (int16_t)w, (int16_t)h);
}

void BoardHAL::displayPushPixelsDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                                    const uint16_t* data) {
  displayPushPixels(x, y, w, h, data);
}

void BoardHAL::displayWaitDMA() {
}

void BoardHAL::displayFillScreen(uint16_t color) {
  if (!g_gfx) {
    return;
  }

  g_gfx->fillScreen(color);
  g_gfx->flush();
}

void BoardHAL::setBrightness(uint8_t value) {
  g_brightness = value;
  apply_backlight(value);
}

uint8_t BoardHAL::getBrightness() {
  return g_brightness;
}

bool BoardHAL::getTouch(TouchPoint* tp) {
  if (!g_touch || !tp) {
    return false;
  }

  uint16_t x[1] = {0};
  uint16_t y[1] = {0};
  uint16_t strength[1] = {0};
  uint8_t count = 0;

  esp_lcd_touch_read_data(g_touch);
  const bool touched = esp_lcd_touch_get_coordinates(g_touch, x, y, strength, &count, 1);
  if (!touched || count == 0) {
    return false;
  }

  tp->x = (int16_t)x[0];
  tp->y = (int16_t)y[0];
  return true;
}

void BoardHAL::displaySleep() {
  if (g_gfx) {
    g_gfx->displayOff();
  }
  apply_backlight(0);
}

void BoardHAL::displayWake() {
  if (g_gfx) {
    g_gfx->displayOn();
    g_gfx->flush();
  }
  apply_backlight(g_brightness);
}

void BoardHAL::displayPowerSaveOn() {
  apply_backlight(0);
}

void BoardHAL::displayPowerSaveOff() {
  apply_backlight(g_brightness);
}

void BoardHAL::displayWaitDisplay() {
  if (g_gfx) {
    g_gfx->flush();
  }
}

bool BoardHAL::initSDCard() {
  if (g_sd_available && SD_MMC.cardType() != CARD_NONE) {
    return true;
  }

  const uint32_t now = millis();
  if (g_sd_init_attempted && (now - g_sd_retry_tick_ms) < 1500) {
    return false;
  }
  g_sd_init_attempted = true;
  g_sd_retry_tick_ms = now;

  SD_MMC.end();

  // Use the official Waveshare 4B SDMMC wiring on slot 0 / IO-MUX.
  // Start conservatively at default speed (20 MHz) for stability.
  if (!SD_MMC.begin("/sdcard", false, SDMMC_FREQ_DEFAULT, 5)) {
    g_sd_available = false;
    Serial.println("[HAL] SD Card mount failed (SDMMC slot 0)");
    return false;
  }

  const uint8_t card_type = SD_MMC.cardType();
  if (card_type == CARD_NONE) {
    g_sd_available = false;
    Serial.println("[HAL] SD Card absent after mount");
    SD_MMC.end();
    return false;
  }

  g_sd_available = true;
  Serial.printf("[HAL] SD Card OK (SDMMC), type=%u, size=%llu MB\n",
                static_cast<unsigned>(card_type),
                static_cast<unsigned long long>(SD_MMC.cardSize() / (1024ULL * 1024ULL)));
  return true;
}
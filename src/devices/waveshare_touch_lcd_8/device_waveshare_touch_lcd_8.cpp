#include "src/devices/waveshare_touch_lcd_8/device_waveshare_touch_lcd_8.h"

#include "src/devices/device_select.h"

#if defined(DEVICE_WAVESHARE_TOUCH_LCD_8)

#include <Arduino.h>
#include <LittleFS.h>
#include <cstring>
#include <utility>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_cache.h>
#include <esp_heap_caps.h>
#include <esp_ldo_regulator.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_mipi_dsi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <driver/ppa.h>
#include <hal/lcd_types.h>

#include "src/devices/waveshare_touch_lcd_8/waveshare_sdmmc.h"
#include "src/devices/waveshare_touch_lcd_8/vendor/displays_config.h"
#include "src/devices/waveshare_touch_lcd_8/vendor/gt911.h"
#include "src/devices/waveshare_touch_lcd_8/vendor/i2c.h"
#include "src/devices/waveshare_touch_lcd_8/vendor/jd9365/esp_lcd_jd9365.h"

namespace {

constexpr gpio_num_t kBacklightPin = GPIO_NUM_26;
constexpr ledc_channel_t kBacklightChannel = LEDC_CHANNEL_0;
constexpr ledc_timer_t kBacklightTimer = LEDC_TIMER_1;
constexpr uint32_t kBacklightFreq = 5000;
constexpr ledc_timer_bit_t kBacklightBits = LEDC_TIMER_10_BIT;
constexpr bool kBacklightActiveLow = false;
constexpr uint8_t kBacklightInputMin = 121;
constexpr uint8_t kBacklightInputMax = 255;
constexpr uint32_t kPanelLaneCount = 2;
constexpr uint32_t kPanelFrameBufferCount = 2;
constexpr size_t kFillChunkRows = 40;
constexpr uintptr_t kCacheLineSize = 64;

esp_lcd_dsi_bus_handle_t g_dsi_bus = nullptr;
esp_lcd_panel_io_handle_t g_panel_io = nullptr;
esp_lcd_panel_handle_t g_panel = nullptr;
esp_lcd_touch_handle_t g_touch = nullptr;
esp_ldo_channel_handle_t g_mipi_phy_ldo = nullptr;
ppa_client_handle_t g_ppa_handle = nullptr;
SemaphoreHandle_t g_transfer_done = nullptr;
SemaphoreHandle_t g_refresh_done = nullptr;

DEV_I2C_Port g_i2c = {};
bool g_i2c_ready = false;
bool g_pmic_ready = false;
uint16_t* g_rotate_buf = nullptr;
size_t g_rotate_buf_pixels = 0;
uint16_t* g_panel_fbs[kPanelFrameBufferCount] = {nullptr, nullptr};
uint8_t g_front_fb = 0;
uint8_t g_back_fb = 1;
bool g_triple_fb_ready = false;
bool g_frame_dirty = false;
int32_t g_dirty_x1 = 0;
int32_t g_dirty_y1 = 0;
int32_t g_dirty_x2 = 0;
int32_t g_dirty_y2 = 0;

uint8_t g_brightness = 200;
bool g_backlight_ready = false;
bool g_sd_init_attempted = false;
bool g_sd_available = false;
uint32_t g_sd_retry_tick_ms = 0;
bool g_littlefs_ready = false;
uint8_t g_rotation = DeviceWaveshareTouchLCD8::kProfile.rotation_default;

void log_step(const char* message) {
  Serial.print("[Device/WaveshareTouchLCD8] ");
  Serial.println(message);
  Serial.flush();
}

bool IRAM_ATTR on_color_trans_done(esp_lcd_panel_handle_t, esp_lcd_dpi_panel_event_data_t*, void* user_ctx) {
  SemaphoreHandle_t sem = static_cast<SemaphoreHandle_t>(user_ctx);
  if (!sem) {
    return false;
  }

  BaseType_t high_task_woken = pdFALSE;
  xSemaphoreGiveFromISR(sem, &high_task_woken);
  return high_task_woken == pdTRUE;
}

bool IRAM_ATTR on_refresh_done(esp_lcd_panel_handle_t, esp_lcd_dpi_panel_event_data_t*, void*) {
  if (!g_refresh_done) {
    return false;
  }

  BaseType_t high_task_woken = pdFALSE;
  xSemaphoreGiveFromISR(g_refresh_done, &high_task_woken);
  return high_task_woken == pdTRUE;
}

void drain_transfer_signal() {
  if (!g_transfer_done) {
    return;
  }
  while (xSemaphoreTake(g_transfer_done, 0) == pdTRUE) {
  }
}

void wait_transfer_done(size_t pixels) {
  if (!g_transfer_done) {
    return;
  }

  const TickType_t timeout = pdMS_TO_TICKS(pixels >= 32768 ? 250 : 50);
  if (xSemaphoreTake(g_transfer_done, timeout) != pdTRUE) {
    Serial.printf("[Device/WaveshareTouchLCD8] draw transfer timeout (%u px)\n",
                  static_cast<unsigned>(pixels));
  }
}

void drain_refresh_signal() {
  if (!g_refresh_done) {
    return;
  }
  while (xSemaphoreTake(g_refresh_done, 0) == pdTRUE) {
  }
}

void wait_refresh_done() {
  if (!g_refresh_done) {
    return;
  }

  if (xSemaphoreTake(g_refresh_done, pdMS_TO_TICKS(50)) != pdTRUE) {
    Serial.println("[Device/WaveshareTouchLCD8] refresh timeout");
  }
}

void flush_cache_for_dma(const void* ptr, size_t size) {
  if (!ptr || size == 0) {
    return;
  }
  const uintptr_t start = reinterpret_cast<uintptr_t>(ptr);
  const uintptr_t aligned_start = start & ~(kCacheLineSize - 1);
  const uintptr_t end = start + size;
  const uintptr_t aligned_end = (end + kCacheLineSize - 1) & ~(kCacheLineSize - 1);
  if (aligned_end <= aligned_start) {
    return;
  }
  esp_cache_msync(reinterpret_cast<void*>(aligned_start),
                  aligned_end - aligned_start,
                  ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
}

bool ensure_rotate_buffer(size_t pixels) {
  if (pixels == 0) {
    return false;
  }
  if (g_rotate_buf && g_rotate_buf_pixels >= pixels) {
    return true;
  }

  if (g_rotate_buf) {
    heap_caps_free(g_rotate_buf);
    g_rotate_buf = nullptr;
    g_rotate_buf_pixels = 0;
  }

  const size_t bytes = pixels * sizeof(uint16_t);
  g_rotate_buf = static_cast<uint16_t*>(
      heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA));
  if (!g_rotate_buf) {
    g_rotate_buf = static_cast<uint16_t*>(
        heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
  }
  if (!g_rotate_buf) {
    Serial.printf("[Device/WaveshareTouchLCD8] Rotate buffer allocation failed (%u bytes)\n",
                  static_cast<unsigned>(bytes));
    return false;
  }

  g_rotate_buf_pixels = pixels;
  return true;
}

bool draw_physical(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data) {
  if (!g_panel || !data || w <= 0 || h <= 0) {
    return false;
  }

  const int32_t panel_w = display_cfg.width;
  const int32_t panel_h = display_cfg.height;
  if (x < 0 || y < 0 || (x + w) > panel_w || (y + h) > panel_h) {
    Serial.printf("[Device/WaveshareTouchLCD8] Reject out-of-range draw x=%ld y=%ld w=%ld h=%ld\n",
                  static_cast<long>(x), static_cast<long>(y),
                  static_cast<long>(w), static_cast<long>(h));
    return false;
  }

  drain_transfer_signal();
  flush_cache_for_dma(data, static_cast<size_t>(w) * static_cast<size_t>(h) * sizeof(uint16_t));
  const esp_err_t err = esp_lcd_panel_draw_bitmap(g_panel, x, y, x + w, y + h, data);
  if (err != ESP_OK) {
    Serial.printf("[Device/WaveshareTouchLCD8] draw_bitmap failed: %d\n", static_cast<int>(err));
    return false;
  }
  wait_transfer_done(static_cast<size_t>(w) * static_cast<size_t>(h));
  return true;
}

size_t panel_pixel_count() {
  return static_cast<size_t>(display_cfg.width) * static_cast<size_t>(display_cfg.height);
}

size_t panel_frame_bytes() {
  return panel_pixel_count() * sizeof(uint16_t);
}

uint16_t* front_fb() {
  return g_triple_fb_ready ? g_panel_fbs[g_front_fb] : nullptr;
}

uint16_t* back_fb() {
  return g_triple_fb_ready ? g_panel_fbs[g_back_fb] : nullptr;
}

void reset_dirty_rect() {
  g_frame_dirty = false;
  g_dirty_x1 = 0;
  g_dirty_y1 = 0;
  g_dirty_x2 = 0;
  g_dirty_y2 = 0;
}

void mark_dirty_rect(int32_t x, int32_t y, int32_t w, int32_t h) {
  if (!g_frame_dirty) {
    g_dirty_x1 = x;
    g_dirty_y1 = y;
    g_dirty_x2 = x + w - 1;
    g_dirty_y2 = y + h - 1;
    g_frame_dirty = true;
    return;
  }

  if (x < g_dirty_x1) g_dirty_x1 = x;
  if (y < g_dirty_y1) g_dirty_y1 = y;
  const int32_t x2 = x + w - 1;
  const int32_t y2 = y + h - 1;
  if (x2 > g_dirty_x2) g_dirty_x2 = x2;
  if (y2 > g_dirty_y2) g_dirty_y2 = y2;
}

void flush_framebuffer_rect(uint16_t* fb, int32_t x, int32_t y, int32_t w, int32_t h) {
  if (!fb || w <= 0 || h <= 0) {
    return;
  }

  const size_t stride = display_cfg.width;
  const size_t row_bytes = static_cast<size_t>(w) * sizeof(uint16_t);
  for (int32_t row = 0; row < h; ++row) {
    flush_cache_for_dma(fb + (static_cast<size_t>(y + row) * stride) + static_cast<size_t>(x),
                        row_bytes);
  }
}

void copy_rect_to_fb(uint16_t* dst, int32_t x, int32_t y, int32_t w, int32_t h,
                     const uint16_t* src) {
  if (!dst || !src || w <= 0 || h <= 0) {
    return;
  }

  const size_t stride = display_cfg.width;
  const size_t row_bytes = static_cast<size_t>(w) * sizeof(uint16_t);
  for (int32_t row = 0; row < h; ++row) {
    std::memcpy(dst + (static_cast<size_t>(y + row) * stride) + static_cast<size_t>(x),
                src + static_cast<size_t>(row) * static_cast<size_t>(w),
                row_bytes);
  }
  flush_framebuffer_rect(dst, x, y, w, h);
}

void copy_rect_between_fbs(uint16_t* dst, const uint16_t* src,
                           int32_t x, int32_t y, int32_t w, int32_t h) {
  if (!dst || !src || w <= 0 || h <= 0) {
    return;
  }

  const size_t stride = display_cfg.width;
  const size_t row_bytes = static_cast<size_t>(w) * sizeof(uint16_t);
  for (int32_t row = 0; row < h; ++row) {
    const size_t offset = (static_cast<size_t>(y + row) * stride) + static_cast<size_t>(x);
    std::memcpy(dst + offset, src + offset, row_bytes);
  }
  flush_framebuffer_rect(dst, x, y, w, h);
}

bool init_triple_framebuffers() {
  if (!g_panel) {
    return false;
  }

  void* fb0 = nullptr;
  void* fb1 = nullptr;
  const esp_err_t err = esp_lcd_dpi_panel_get_frame_buffer(g_panel,
                                                           kPanelFrameBufferCount,
                                                           &fb0, &fb1);
  if (err != ESP_OK || !fb0 || !fb1) {
    Serial.printf("[Device/WaveshareTouchLCD8] Double framebuffer unavailable err=%d fb=%p/%p\n",
                  static_cast<int>(err), fb0, fb1);
    g_triple_fb_ready = false;
    return false;
  }

  g_panel_fbs[0] = static_cast<uint16_t*>(fb0);
  g_panel_fbs[1] = static_cast<uint16_t*>(fb1);
  g_front_fb = 0;
  g_back_fb = 1;
  g_triple_fb_ready = true;
  reset_dirty_rect();
  Serial.printf("[Device/WaveshareTouchLCD8] Double framebuffer OK fb=%p/%p\n", fb0, fb1);
  return true;
}

bool write_physical_to_back(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data) {
  if (!g_triple_fb_ready || !data || w <= 0 || h <= 0) {
    return false;
  }

  const int32_t panel_w = display_cfg.width;
  const int32_t panel_h = display_cfg.height;
  if (x < 0 || y < 0 || (x + w) > panel_w || (y + h) > panel_h) {
    Serial.printf("[Device/WaveshareTouchLCD8] Reject out-of-range backbuffer draw x=%ld y=%ld w=%ld h=%ld\n",
                  static_cast<long>(x), static_cast<long>(y),
                  static_cast<long>(w), static_cast<long>(h));
    return false;
  }

  uint16_t* back = back_fb();
  if (!back) {
    return false;
  }

  copy_rect_to_fb(back, x, y, w, h, data);
  mark_dirty_rect(x, y, w, h);
  return true;
}

bool draw_landscape_area(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data) {
  if (!data || w <= 0 || h <= 0) {
    return false;
  }

  const int32_t logical_w = display_cfg.height;
  const int32_t logical_h = display_cfg.width;
  if (x < 0 || y < 0 || (x + w) > logical_w || (y + h) > logical_h) {
    Serial.printf("[Device/WaveshareTouchLCD8] Reject out-of-range landscape draw x=%ld y=%ld w=%ld h=%ld\n",
                  static_cast<long>(x), static_cast<long>(y),
                  static_cast<long>(w), static_cast<long>(h));
    return false;
  }

  int32_t dst_x = 0;
  int32_t dst_y = 0;
  const int32_t dst_w = h;
  const int32_t dst_h = w;
  if (g_rotation & 0x02) {
    dst_x = y;
    dst_y = logical_w - x - w;
  } else {
    dst_x = logical_h - y - h;
    dst_y = x;
  }

  if (g_ppa_handle && g_triple_fb_ready) {
    uint16_t* back = back_fb();
    if (back) {
      flush_cache_for_dma(data, static_cast<size_t>(w) * static_cast<size_t>(h) * sizeof(uint16_t));

      ppa_srm_oper_config_t oper = {};
      oper.in.buffer = data;
      oper.in.pic_w = w;
      oper.in.pic_h = h;
      oper.in.block_w = w;
      oper.in.block_h = h;
      oper.in.block_offset_x = 0;
      oper.in.block_offset_y = 0;
      oper.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;

      oper.out.buffer = back;
      oper.out.buffer_size = panel_frame_bytes();
      oper.out.pic_w = display_cfg.width;
      oper.out.pic_h = display_cfg.height;
      oper.out.block_offset_x = dst_x;
      oper.out.block_offset_y = dst_y;
      oper.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;

      oper.rotation_angle = (g_rotation & 0x02) ? PPA_SRM_ROTATION_ANGLE_90
                                                : PPA_SRM_ROTATION_ANGLE_270;
      oper.scale_x = 1.0f;
      oper.scale_y = 1.0f;
      oper.rgb_swap = false;
      oper.byte_swap = false;
      oper.mode = PPA_TRANS_MODE_BLOCKING;

      const esp_err_t err = ppa_do_scale_rotate_mirror(g_ppa_handle, &oper);
      if (err == ESP_OK) {
        mark_dirty_rect(dst_x, dst_y, dst_w, dst_h);
        return true;
      }
      Serial.printf("[Device/WaveshareTouchLCD8] PPA rotate failed err=%d, falling back to CPU\n",
                    static_cast<int>(err));
    }
  }

  const size_t pixel_count = static_cast<size_t>(w) * static_cast<size_t>(h);
  if (!ensure_rotate_buffer(pixel_count)) {
    return false;
  }

  if (g_rotation & 0x02) {
    for (int32_t sy = 0; sy < h; ++sy) {
      const uint16_t* src_row = data + static_cast<size_t>(sy) * w;
      for (int32_t sx = 0; sx < w; ++sx) {
        g_rotate_buf[static_cast<size_t>(w - 1 - sx) * h + sy] = src_row[sx];
      }
    }
  } else {
    for (int32_t sy = 0; sy < h; ++sy) {
      const uint16_t* src_row = data + static_cast<size_t>(sy) * w;
      for (int32_t sx = 0; sx < w; ++sx) {
        g_rotate_buf[static_cast<size_t>(sx) * h + (h - 1 - sy)] = src_row[sx];
      }
    }
  }

  if (write_physical_to_back(dst_x, dst_y, dst_w, dst_h, g_rotate_buf)) {
    return true;
  }

  return draw_physical(dst_x, dst_y, dst_w, dst_h, g_rotate_buf);
}

void hold_panel_reset_low() {
  if (display_cfg.lcd_rst < 0) {
    return;
  }

  const gpio_num_t rst_pin = static_cast<gpio_num_t>(display_cfg.lcd_rst);
  gpio_set_direction(rst_pin, GPIO_MODE_OUTPUT);
  gpio_set_level(rst_pin, 0);
}

void apply_backlight(uint8_t value) {
  if (!g_backlight_ready) {
    return;
  }

  uint32_t scaled = value;
  if (value == 0) {
    scaled = 0;
  } else if (value <= kBacklightInputMin) {
    scaled = 1;
  } else if (value >= kBacklightInputMax) {
    scaled = 255;
  } else {
    const uint32_t span = static_cast<uint32_t>(kBacklightInputMax - kBacklightInputMin);
    scaled = 1u + ((static_cast<uint32_t>(value - kBacklightInputMin) * 254u) + (span / 2u)) / span;
  }

  constexpr uint32_t kMaxDuty = (1u << 10) - 1u;
  uint32_t duty = (scaled * kMaxDuty + 127u) / 255u;
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

  gpio_set_direction(kBacklightPin, GPIO_MODE_OUTPUT);
  gpio_set_level(kBacklightPin, kBacklightActiveLow ? 1 : 0);

  ledc_timer_config_t timer_cfg = {};
  timer_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
  timer_cfg.timer_num = kBacklightTimer;
  timer_cfg.duty_resolution = kBacklightBits;
  timer_cfg.freq_hz = kBacklightFreq;
  timer_cfg.clk_cfg = LEDC_AUTO_CLK;
  if (ledc_timer_config(&timer_cfg) != ESP_OK) {
    Serial.println("[Device/WaveshareTouchLCD8] Backlight timer init failed");
    return false;
  }

  ledc_channel_config_t ch_cfg = {};
  ch_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
  ch_cfg.channel = kBacklightChannel;
  ch_cfg.timer_sel = kBacklightTimer;
  ch_cfg.gpio_num = kBacklightPin;
  ch_cfg.duty = kBacklightActiveLow ? ((1u << 10) - 1u) : 0;
  ch_cfg.hpoint = 0;
  if (ledc_channel_config(&ch_cfg) != ESP_OK) {
    Serial.println("[Device/WaveshareTouchLCD8] Backlight channel init failed");
    return false;
  }

  g_backlight_ready = true;
  apply_backlight(0);
  return true;
}

bool init_i2c() {
  if (g_i2c_ready) {
    return true;
  }

  g_i2c = DEV_I2C_Init();
  if (!g_i2c.bus) {
    Serial.println("[Device/WaveshareTouchLCD8] I2C init failed");
    return false;
  }
  g_i2c_ready = true;
  return true;
}

bool init_pmic() {
  if (g_pmic_ready) {
    return true;
  }

  // The standalone 8-DSI-TOUCH-A panel wiki documents I2C addr 0x45 for
  // backlight control when paired with an ESP32-P4-NANO carrier.
  // The integrated ESP32-P4-WIFI6-Touch-LCD-X BSP instead exposes a dedicated
  // backlight GPIO (GPIO26) and shares I2C with touch/audio devices.
  // Probing 0x45 on this board only adds noise and can send us down the wrong
  // hardware path, so keep PMIC init as a no-op here unless Waveshare publish
  // board-specific proof that the WIFI6 8" variant really needs it.
  g_pmic_ready = true;
  return true;
}

bool init_mipi_phy_power() {
  if (g_mipi_phy_ldo) {
    return true;
  }

  esp_ldo_channel_config_t ldo_cfg = {};
  ldo_cfg.chan_id = 3;
  ldo_cfg.voltage_mv = 2500;

  log_step("MIPI PHY LDO init start");
  const esp_err_t err = esp_ldo_acquire_channel(&ldo_cfg, &g_mipi_phy_ldo);
  if (err != ESP_OK) {
    Serial.printf("[Device/WaveshareTouchLCD8] MIPI PHY LDO init failed err=%d\n",
                  static_cast<int>(err));
    return false;
  }
  log_step("MIPI PHY LDO init OK");
  return true;
}

bool init_display() {
  if (g_panel) {
    return true;
  }
  if (!init_pmic()) {
    return false;
  }
  if (!init_mipi_phy_power()) {
    return false;
  }

  log_step("Allocating transfer semaphore");
  g_transfer_done = xSemaphoreCreateBinary();
  g_refresh_done = xSemaphoreCreateBinary();
  if (!g_transfer_done || !g_refresh_done) {
    Serial.println("[Device/WaveshareTouchLCD8] Transfer semaphore allocation failed");
    return false;
  }
  log_step("Transfer semaphore OK");

  esp_lcd_dsi_bus_config_t bus_cfg = {};
  bus_cfg.bus_id = 0;
  bus_cfg.num_data_lanes = kPanelLaneCount;
  // The Arduino board must be selected as "Before v3.00" for this hardware.
  // That ESP32-P4 path accepts PLL_F20M/RC_FAST/PLL_F25M only; XTAL/default
  // aborts in the low-level MIPI DSI clock-source switch.
  bus_cfg.phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M;
  bus_cfg.lane_bit_rate_mbps = static_cast<float>(display_cfg.lane_bit_rate);
  Serial.printf("[Device/WaveshareTouchLCD8] DSI bus init start lane=%u Mbps\n",
                static_cast<unsigned>(display_cfg.lane_bit_rate));
  Serial.flush();
  esp_err_t err = esp_lcd_new_dsi_bus(&bus_cfg, &g_dsi_bus);
  if (err != ESP_OK) {
    Serial.printf("[Device/WaveshareTouchLCD8] DSI bus init failed err=%d\n", static_cast<int>(err));
    return false;
  }
  log_step("DSI bus init OK");

  esp_lcd_dbi_io_config_t dbi_cfg = {};
  dbi_cfg.virtual_channel = 0;
  dbi_cfg.lcd_cmd_bits = 8;
  dbi_cfg.lcd_param_bits = 8;
  log_step("DSI DBI IO init start");
  err = esp_lcd_new_panel_io_dbi(g_dsi_bus, &dbi_cfg, &g_panel_io);
  if (err != ESP_OK) {
    Serial.printf("[Device/WaveshareTouchLCD8] DSI DBI IO init failed err=%d\n", static_cast<int>(err));
    return false;
  }
  log_step("DSI DBI IO init OK");

  esp_lcd_dpi_panel_config_t dpi_cfg = {};
  dpi_cfg.virtual_channel = 0;
  dpi_cfg.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
  dpi_cfg.dpi_clock_freq_mhz = static_cast<float>(display_cfg.prefer_speed) / 1000000.0f;
  dpi_cfg.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
  dpi_cfg.in_color_format = LCD_COLOR_FMT_RGB565;
  dpi_cfg.out_color_format = LCD_COLOR_FMT_RGB565;
  dpi_cfg.num_fbs = kPanelFrameBufferCount;
  dpi_cfg.video_timing.h_size = display_cfg.width;
  dpi_cfg.video_timing.v_size = display_cfg.height;
  dpi_cfg.video_timing.hsync_pulse_width = display_cfg.hsync_pulse_width;
  dpi_cfg.video_timing.hsync_back_porch = display_cfg.hsync_back_porch;
  dpi_cfg.video_timing.hsync_front_porch = display_cfg.hsync_front_porch;
  dpi_cfg.video_timing.vsync_pulse_width = display_cfg.vsync_pulse_width;
  dpi_cfg.video_timing.vsync_back_porch = display_cfg.vsync_back_porch;
  dpi_cfg.video_timing.vsync_front_porch = display_cfg.vsync_front_porch;
  // Offload the commit_fb → active panel FB copy to DMA2D. The fallback
  // draw_physical() path still waits for on_color_trans_done before the
  // reused rotate buffer is overwritten, so DMA2D is safe here.
  dpi_cfg.flags.use_dma2d = true;

  jd9365_vendor_config_t vendor_cfg = {};
  vendor_cfg.init_cmds = nullptr;
  vendor_cfg.init_cmds_size = 0;
  vendor_cfg.mipi_config.dsi_bus = g_dsi_bus;
  vendor_cfg.mipi_config.dpi_config = &dpi_cfg;
  vendor_cfg.mipi_config.lane_num = kPanelLaneCount;

  esp_lcd_panel_dev_config_t panel_cfg = {};
  panel_cfg.reset_gpio_num = display_cfg.lcd_rst;
  panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  panel_cfg.data_endian = LCD_RGB_DATA_ENDIAN_LITTLE;
  panel_cfg.bits_per_pixel = 16;
  panel_cfg.flags.reset_active_high = false;
  panel_cfg.vendor_config = &vendor_cfg;

  log_step("JD9365 panel create start");
  err = esp_lcd_new_panel_jd9365(g_panel_io, &panel_cfg, &g_panel);
  if (err != ESP_OK) {
    Serial.printf("[Device/WaveshareTouchLCD8] JD9365 panel create failed err=%d\n", static_cast<int>(err));
    return false;
  }
  log_step("JD9365 panel create OK");

  init_triple_framebuffers();

  esp_lcd_dpi_panel_event_callbacks_t cbs = {};
  cbs.on_color_trans_done = on_color_trans_done;
  cbs.on_refresh_done = on_refresh_done;
  log_step("DPI callback register start");
  err = esp_lcd_dpi_panel_register_event_callbacks(g_panel, &cbs, g_transfer_done);
  if (err != ESP_OK) {
    Serial.printf("[Device/WaveshareTouchLCD8] DPI callback register failed err=%d\n", static_cast<int>(err));
    return false;
  }
  log_step("DPI callback register OK");

  log_step("Panel reset start");
  err = esp_lcd_panel_reset(g_panel);
  if (err != ESP_OK) {
    Serial.printf("[Device/WaveshareTouchLCD8] Panel reset failed err=%d\n", static_cast<int>(err));
    return false;
  }
  log_step("Panel reset OK");

  log_step("Panel init start");
  err = esp_lcd_panel_init(g_panel);
  if (err != ESP_OK) {
    Serial.printf("[Device/WaveshareTouchLCD8] Panel init failed err=%d\n", static_cast<int>(err));
    return false;
  }
  log_step("Panel init OK");

  log_step("Panel display on start");
  err = esp_lcd_panel_disp_on_off(g_panel, true);
  if (err != ESP_OK) {
    Serial.printf("[Device/WaveshareTouchLCD8] Panel display on failed err=%d\n", static_cast<int>(err));
    return false;
  }
  log_step("Panel display on OK");

  ppa_client_config_t ppa_cfg = {};
  ppa_cfg.oper_type = PPA_OPERATION_SRM;
  const esp_err_t ppa_err = ppa_register_client(&ppa_cfg, &g_ppa_handle);
  if (ppa_err != ESP_OK) {
    Serial.printf("[Device/WaveshareTouchLCD8] PPA client register failed err=%d, falling back to CPU rotate\n",
                  static_cast<int>(ppa_err));
    g_ppa_handle = nullptr;
  } else {
    log_step("PPA client registered");
  }

  return true;
}

bool init_touch() {
  if (g_touch) {
    return true;
  }
  if (!init_i2c()) {
    return false;
  }

  for (uint8_t attempt = 1; attempt <= 3; ++attempt) {
    g_touch = touch_gt911_init(g_i2c);
    if (g_touch) {
      return true;
    }
    Serial.printf("[Device/WaveshareTouchLCD8] Touch init attempt %u failed\n",
                  static_cast<unsigned>(attempt));
    delay(150);
  }

  if (!g_touch) {
    Serial.println("[Device/WaveshareTouchLCD8] Touch init failed");
    return false;
  }

  return true;
}

void ensure_storage_layout() {
  if (!g_littlefs_ready) {
    return;
  }
  LittleFS.mkdir("/_tile_grids");
  LittleFS.mkdir("/_tile_links");
  LittleFS.mkdir("/icons");
}

}  // namespace

bool DeviceWaveshareTouchLCD8::init() {
  Serial.println("[Device/WaveshareTouchLCD8] Initialising board...");

  if (!init_backlight()) {
    return false;
  }
  apply_backlight(0);

  if (!init_display()) {
    return false;
  }
  Serial.println("[Device/WaveshareTouchLCD8] Display OK");

  if (!init_touch()) {
    Serial.println("[Device/WaveshareTouchLCD8] Touch init failed, continuing without touch");
  } else {
    Serial.println("[Device/WaveshareTouchLCD8] Touch OK");
  }

  displayFillScreen(0x0000);
  setBrightness(g_brightness);
  Serial.println("[Device/WaveshareTouchLCD8] Init complete");
  return true;
}

void DeviceWaveshareTouchLCD8::update() {
}

void DeviceWaveshareTouchLCD8::displayPushPixels(int32_t x, int32_t y, int32_t w, int32_t h,
                                                 const uint16_t* data) {
  draw_landscape_area(x, y, w, h, data);
}

void DeviceWaveshareTouchLCD8::displayPushPixelsDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                                                    const uint16_t* data) {
  draw_landscape_area(x, y, w, h, data);
}

void DeviceWaveshareTouchLCD8::displayWaitDMA() {
}

void DeviceWaveshareTouchLCD8::displayCommit() {
  if (!g_panel || !g_triple_fb_ready || !g_frame_dirty) {
    return;
  }

  uint16_t* commit_fb = back_fb();
  if (!commit_fb) {
    return;
  }

  const int32_t dirty_w = g_dirty_x2 - g_dirty_x1 + 1;
  const int32_t dirty_h = g_dirty_y2 - g_dirty_y1 + 1;
  flush_framebuffer_rect(commit_fb, g_dirty_x1, g_dirty_y1, dirty_w, dirty_h);
  // Driver swaps FBs on next VSync itself — don't block the LVGL/touch task here.
  const esp_err_t err = esp_lcd_panel_draw_bitmap(g_panel,
                                                  0,
                                                  0,
                                                  display_cfg.width,
                                                  display_cfg.height,
                                                  commit_fb);
  if (err != ESP_OK) {
    Serial.printf("[Device/WaveshareTouchLCD8] commit failed: %d\n", static_cast<int>(err));
    return;
  }

  std::swap(g_front_fb, g_back_fb);

  // Mirror the just-committed dirty rect from the new front into the new back
  // so LVGL's next partial flush starts from the currently displayed content.
  copy_rect_between_fbs(back_fb(), front_fb(), g_dirty_x1, g_dirty_y1, dirty_w, dirty_h);
  reset_dirty_rect();
}

void DeviceWaveshareTouchLCD8::displayFillScreen(uint16_t color) {
  if (!g_panel) {
    return;
  }

  if (g_triple_fb_ready) {
    const size_t pixels = panel_pixel_count();
    for (uint8_t fb = 0; fb < kPanelFrameBufferCount; ++fb) {
      uint16_t* buf = g_panel_fbs[fb];
      if (!buf) {
        continue;
      }
      for (size_t i = 0; i < pixels; ++i) {
        buf[i] = color;
      }
      flush_cache_for_dma(buf, panel_frame_bytes());
    }

    const esp_err_t err = esp_lcd_panel_draw_bitmap(g_panel,
                                                    0,
                                                    0,
                                                    display_cfg.width,
                                                    display_cfg.height,
                                                    front_fb());
    if (err != ESP_OK) {
      Serial.printf("[Device/WaveshareTouchLCD8] triple fill commit failed: %d\n", static_cast<int>(err));
    }
    reset_dirty_rect();
    return;
  }

  const size_t chunk_pixels = static_cast<size_t>(display_cfg.width) * kFillChunkRows;
  if (!ensure_rotate_buffer(chunk_pixels)) {
    return;
  }

  for (size_t i = 0; i < chunk_pixels; ++i) {
    g_rotate_buf[i] = color;
  }

  for (int32_t y = 0; y < display_cfg.height; y += static_cast<int32_t>(kFillChunkRows)) {
    int32_t rows = static_cast<int32_t>(kFillChunkRows);
    if (y + rows > display_cfg.height) {
      rows = display_cfg.height - y;
    }
    draw_physical(0, y, display_cfg.width, rows, g_rotate_buf);
  }
}

void DeviceWaveshareTouchLCD8::displaySetRotation(uint8_t rotation) {
  g_rotation = rotation & 0x03;
}

void DeviceWaveshareTouchLCD8::setBrightness(uint8_t value) {
  g_brightness = value;
  apply_backlight(value);
}

uint8_t DeviceWaveshareTouchLCD8::getBrightness() {
  return g_brightness;
}

bool DeviceWaveshareTouchLCD8::getTouch(int16_t& x, int16_t& y) {
  if (!g_touch) {
    return false;
  }

  uint16_t px[1] = {0};
  uint16_t py[1] = {0};
  uint16_t strength[1] = {0};
  uint8_t count = 0;

  esp_lcd_touch_read_data(g_touch);
  const bool touched = esp_lcd_touch_get_coordinates(g_touch, px, py, strength, &count, 1);
  if (!touched || count == 0) {
    return false;
  }

  int32_t mapped_x = 0;
  int32_t mapped_y = 0;

  if (g_rotation & 0x02) {
    mapped_x = static_cast<int32_t>(display_cfg.height) - 1 - static_cast<int32_t>(py[0]);
    mapped_y = static_cast<int32_t>(px[0]);
  } else {
    mapped_x = static_cast<int32_t>(py[0]);
    mapped_y = static_cast<int32_t>(display_cfg.width) - 1 - static_cast<int32_t>(px[0]);
  }

  const int32_t logical_w = static_cast<int32_t>(display_cfg.height);
  const int32_t logical_h = static_cast<int32_t>(display_cfg.width);

  if (mapped_x < 0) mapped_x = 0;
  if (mapped_y < 0) mapped_y = 0;
  if (mapped_x >= logical_w) mapped_x = logical_w - 1;
  if (mapped_y >= logical_h) mapped_y = logical_h - 1;

  x = static_cast<int16_t>(mapped_x);
  y = static_cast<int16_t>(mapped_y);
  return true;
}

void DeviceWaveshareTouchLCD8::displaySleep() {
  if (g_panel) {
    esp_lcd_panel_disp_on_off(g_panel, false);
    esp_lcd_panel_disp_sleep(g_panel, true);
  }
  apply_backlight(0);
}

void DeviceWaveshareTouchLCD8::displayWake() {
  if (g_panel) {
    esp_lcd_panel_disp_sleep(g_panel, false);
    esp_lcd_panel_disp_on_off(g_panel, true);
  }
  apply_backlight(g_brightness);
}

void DeviceWaveshareTouchLCD8::displayPowerSaveOn() {
  displaySleep();
}

void DeviceWaveshareTouchLCD8::displayPowerSaveOff() {
  displayWake();
}

void DeviceWaveshareTouchLCD8::displayWaitDisplay() {
}

void DeviceWaveshareTouchLCD8::prepareForRestart() {
  if (g_panel) {
    displayFillScreen(0x0000);
    esp_lcd_panel_disp_on_off(g_panel, false);
  }

  hold_panel_reset_low();
  apply_backlight(0);
  ledc_stop(LEDC_LOW_SPEED_MODE, kBacklightChannel, kBacklightActiveLow ? 1u : 0u);
  gpio_set_direction(kBacklightPin, GPIO_MODE_OUTPUT);
  gpio_set_level(kBacklightPin, kBacklightActiveLow ? 1 : 0);
}

bool DeviceWaveshareTouchLCD8::initSDCard() {
  if (g_sd_available && WaveshareSDMMC.cardType() != CARD_NONE) {
    return true;
  }

  const uint32_t now = millis();
  if (g_sd_init_attempted && (now - g_sd_retry_tick_ms) < 1500) {
    return false;
  }
  g_sd_init_attempted = true;
  g_sd_retry_tick_ms = now;

  WaveshareSDMMC.end();

  if (!WaveshareSDMMC.begin("/sdcard", false, SDMMC_FREQ_DEFAULT, 5)) {
    g_sd_available = false;
    Serial.println("[Device/WaveshareTouchLCD8] SD card mount failed");
    return false;
  }

  const uint8_t card_type = WaveshareSDMMC.cardType();
  if (card_type == CARD_NONE) {
    g_sd_available = false;
    Serial.println("[Device/WaveshareTouchLCD8] SD card absent after mount");
    WaveshareSDMMC.end();
    return false;
  }

  g_sd_available = true;
  Serial.printf("[Device/WaveshareTouchLCD8] SD card OK, type=%u, size=%llu MB\n",
                static_cast<unsigned>(card_type),
                static_cast<unsigned long long>(WaveshareSDMMC.cardSize() / (1024ULL * 1024ULL)));
  return true;
}

bool DeviceWaveshareTouchLCD8::storageReady() {
  return g_littlefs_ready;
}

fs::FS& DeviceWaveshareTouchLCD8::storageFS() {
  return LittleFS;
}

bool DeviceWaveshareTouchLCD8::sdReady() {
  return initSDCard();
}

fs::FS& DeviceWaveshareTouchLCD8::sdFS() {
  return WaveshareSDMMC;
}

bool DeviceWaveshareTouchLCD8::initLittleFS() {
  if (g_littlefs_ready) {
    return true;
  }
  if (!LittleFS.begin(true, "/littlefs", 10, "spiffs")) {
    Serial.println("[Device/WaveshareTouchLCD8] LittleFS mount failed");
    return false;
  }
  g_littlefs_ready = true;
  Serial.printf("[Device/WaveshareTouchLCD8] LittleFS ready, total=%u, used=%u\n",
                static_cast<unsigned>(LittleFS.totalBytes()),
                static_cast<unsigned>(LittleFS.usedBytes()));
  ensure_storage_layout();
  return true;
}

static bool copyFile(fs::FS& srcFS, fs::FS& dstFS, const char* path) {
  File src = srcFS.open(path, FILE_READ);
  if (!src) {
    return false;
  }
  File dst = dstFS.open(path, FILE_WRITE);
  if (!dst) {
    src.close();
    return false;
  }
  uint8_t buf[512];
  while (src.available()) {
    const size_t n = src.read(buf, sizeof(buf));
    if (n == 0) break;
    dst.write(buf, n);
  }
  dst.close();
  src.close();
  return true;
}

static void copyDirectory(fs::FS& srcFS, fs::FS& dstFS, const char* dirPath) {
  File dir = srcFS.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    return;
  }
  dstFS.mkdir(dirPath);
  File entry = dir.openNextFile();
  while (entry) {
    String fullPath = String(dirPath) + "/" + entry.name();
    if (entry.isDirectory()) {
      copyDirectory(srcFS, dstFS, fullPath.c_str());
    } else {
      copyFile(srcFS, dstFS, fullPath.c_str());
      Serial.printf("[Storage] Migrated: %s (%u bytes)\n", fullPath.c_str(),
                    static_cast<unsigned>(entry.size()));
    }
    entry = dir.openNextFile();
  }
}

void DeviceWaveshareTouchLCD8::migrateStorageFromSD() {
  if (!initLittleFS()) {
    return;
  }
  if (LittleFS.exists("/_migrated")) {
    return;
  }

  LittleFS.mkdir("/_tile_grids");
  LittleFS.mkdir("/_tile_links");
  LittleFS.mkdir("/icons");

  if (initSDCard()) {
    Serial.println("[Storage] Migrating data from SD to LittleFS...");
    copyDirectory(WaveshareSDMMC, LittleFS, "/_tile_grids");
    copyDirectory(WaveshareSDMMC, LittleFS, "/_tile_links");
    copyDirectory(WaveshareSDMMC, LittleFS, "/icons");
    Serial.println("[Storage] Migration complete");
  } else {
    Serial.println("[Storage] No SD card, starting fresh");
  }

  File flag = LittleFS.open("/_migrated", FILE_WRITE);
  if (flag) {
    flag.print("1");
    flag.close();
  }
}

#endif  // defined(DEVICE_WAVESHARE_TOUCH_LCD_8)

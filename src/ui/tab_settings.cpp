#include <lvgl.h>
#include <WiFi.h>
#include "src/ui/tab_settings.h"
#include "src/core/config_manager.h"
#include "src/core/board_hal.h"
#include "src/tiles/mdi_icons.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/ui/ui_manager.h"
#include "src/ui/tab_tiles_unified.h"
#include "src/fonts/ui_fonts.h"
#include "src/core/firmware_version.h"
#include "src/network/mqtt_handlers.h"
#include "src/core/display_manager.h"
#include "src/core/i18n.h"
#include "src/types/clock/clock_format.h"
#include "src/web/web_config.h"
#include "src/ui/ui_keyboard.h"
#include "src/ui/wifi_setup_popup.h"

static lv_obj_t *brightness_label = nullptr;
static lv_obj_t *display_rotate_btn = nullptr;
static lv_obj_t *display_rotate_label = nullptr;
static lv_obj_t *display_rotate_sub_label = nullptr;
static bool display_rotated_180 = false;
static uint8_t display_rotation_quarters = Device::kRotationDefault;
static uint8_t display_rotation_mode = kDisplayRotationNormal;
static lv_obj_t *mains_wake_btn = nullptr;
static lv_obj_t *mains_wake_label = nullptr;
static lv_obj_t *mains_wake_sub_label = nullptr;
// Battery wake removed (no battery on Waveshare)
static lv_obj_t *battery_wake_btn = nullptr;
static lv_obj_t *battery_wake_label = nullptr;
static lv_obj_t *battery_wake_sub_label = nullptr;
static uint8_t wake_mode_mains = kWakeModeTouch;
static uint8_t wake_mode_battery = kWakeModeTouch;
static hotspot_callback_t g_hotspot_callback = nullptr;

enum class SettingsPopupKind : uint8_t {
  Display,
  Wifi,
  Mqtt,
  Localization,
  Firmware,
};

static lv_obj_t *settings_tile_display_title = nullptr;
static lv_obj_t *settings_tile_display_summary = nullptr;
static lv_obj_t *settings_tile_wifi_title = nullptr;
static lv_obj_t *settings_tile_mqtt_title = nullptr;
static lv_obj_t *settings_tile_mqtt_summary = nullptr;
static lv_obj_t *settings_tile_locale_title = nullptr;
static lv_obj_t *settings_tile_locale_summary = nullptr;
static lv_obj_t *settings_tile_firmware_title = nullptr;
static lv_obj_t *settings_tile_firmware_summary = nullptr;

static lv_obj_t *settings_popup_overlay = nullptr;
static lv_obj_t *settings_popup_card = nullptr;
static lv_obj_t *settings_popup_title = nullptr;
static lv_obj_t *settings_popup_save_btn = nullptr;
static lv_obj_t *settings_popup_save_label = nullptr;
static lv_obj_t *settings_popup_content = nullptr;
static lv_obj_t *settings_popup_keyboard = nullptr;
static lv_obj_t *settings_popup_active_ta = nullptr;
static SettingsPopupKind settings_popup_kind = SettingsPopupKind::Display;
static bool settings_popup_restart_pending = false;

static lv_obj_t *wifi_ssid_ta = nullptr;
static lv_obj_t *wifi_pass_ta = nullptr;
static lv_obj_t *wifi_static_cb = nullptr;
static lv_obj_t *wifi_ip_ta = nullptr;
static lv_obj_t *wifi_gateway_ta = nullptr;
static lv_obj_t *wifi_subnet_ta = nullptr;
static lv_obj_t *wifi_dns_ta = nullptr;
static lv_obj_t *wifi_show_pass_cb = nullptr;

static lv_obj_t *mqtt_host_ta = nullptr;
static lv_obj_t *mqtt_port_ta = nullptr;
static lv_obj_t *mqtt_user_ta = nullptr;
static lv_obj_t *mqtt_pass_ta = nullptr;
static lv_obj_t *mqtt_client_id_ta = nullptr;
static lv_obj_t *mqtt_base_ta = nullptr;
static lv_obj_t *mqtt_ha_prefix_ta = nullptr;
static lv_obj_t *mqtt_show_pass_cb = nullptr;

static lv_obj_t *locale_language_dd = nullptr;
static lv_obj_t *locale_timezone_dd = nullptr;
static lv_obj_t *locale_time_format_dd = nullptr;
static lv_obj_t *locale_date_format_dd = nullptr;

static const i18n::Strings& tr() {
  return i18n::strings(configManager.getConfig().language);
}

// WiFi Status Labels
static lv_obj_t *wifi_status_label = nullptr;
static lv_obj_t *wifi_ssid_label = nullptr;
static lv_obj_t *wifi_ip_label = nullptr;
static lv_obj_t *ap_mode_btn = nullptr;
static lv_obj_t *ap_mode_btn_label = nullptr;
static lv_obj_t *wifi_choose_btn = nullptr;
static lv_obj_t *wifi_choose_btn_label = nullptr;
static lv_obj_t *ap_confirm_row = nullptr;
static lv_obj_t *ap_confirm_yes_btn = nullptr;
static lv_obj_t *ap_confirm_no_btn = nullptr;
static lv_obj_t *display_section_label = nullptr;
static lv_obj_t *brightness_title_label = nullptr;
static lv_obj_t *wifi_section_label = nullptr;
static lv_obj_t *ap_yes_label_obj = nullptr;
static lv_obj_t *ap_no_label_obj = nullptr;
static lv_obj_t *sleep_section_label = nullptr;
static lv_timer_t *ap_confirm_timer = nullptr;
static bool ap_mode_confirm_pending = false;
static bool ap_mode_active = false;
static uint32_t ap_mode_click_block_until = 0;

// Sleep Settings
static lv_obj_t *sleep_slider = nullptr;
static lv_obj_t *sleep_time_label = nullptr;
static lv_obj_t *sleep_label = nullptr;

// Power Status Labels (stubs -> no battery display)
static lv_obj_t *power_status_label = nullptr;
static lv_obj_t *power_level_label = nullptr;
static lv_obj_t *battery_icon_label = nullptr;
static lv_obj_t *battery_percent_label = nullptr;

// Layout constants -> kompakter fuer 720x720, 4x4 Grid
static const int kSettingsColLeftPct = 15;
static const int kSettingsColRightPct = 85;
static const int kSettingsColGap = 8;
static const int kSettingsColRowGap = 4;
static const int kSettingsBtnHeight = 80;
static const int kSettingsButtonWidthPct = 90;
static const int kSettingsSliderLabelWidth = 160;
static const int kSettingsSectionTitlePct = 20;
static const int kSettingsSectionContentPct = 50;
static const int kSettingsSectionActionPct = 30;
static const int kSettingsDisplayValueWidth = 56;
static const int kSettingsInlineLabelWidth = 98;
static const int kSettingsInlineSliderWidth = 116;
static const uint8_t kSettingsBrightnessRawMin = 121;
static const uint8_t kSettingsBrightnessRawMax = 255;
static const int kSettingsBrightnessPctMin = 1;
static const int kSettingsBrightnessPctMax = 100;
static const int kSettingsSliderValueWidth = 70;
static const int kSettingsSliderHeight = 16;
static const int kSettingsSliderKnobSize = 36;
static const int kSettingsSliderClickPad = 20;
static const uint8_t kSettingsCardColStart = 1;

// Forward declarations
void settings_update_ap_mode(bool running);
void settings_refresh_language();
static void update_settings_tile_summaries();
static void open_settings_popup(SettingsPopupKind kind);

static void style_settings_button(lv_obj_t *btn, uint32_t base_color) {
  if (!btn) return;
  uint32_t pressed_color = brighten_rgb_color(base_color, 0x10);
  lv_obj_set_style_anim_duration(btn, 0, 0);
  disable_pressed_button_animation(btn);
  lv_obj_set_style_transform_width(btn, 0, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_transform_height(btn, 0, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_translate_x(btn, 0, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_translate_y(btn, 0, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_bg_color(btn, lv_color_hex(base_color), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(btn, lv_color_hex(base_color), LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_bg_grad_color(btn, lv_color_hex(base_color), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_color(btn, lv_color_hex(base_color), LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_bg_grad_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_grad_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
}
static uint16_t sleep_seconds_from_index(int32_t index) {
  if (index < 0) {
    index = 0;
  } else if (index >= static_cast<int32_t>(kSleepOptionsSecCount)) {
    index = static_cast<int32_t>(kSleepOptionsSecCount) - 1;
  }
  return kSleepOptionsSec[index];
}

static int32_t sleep_index_from_seconds(uint16_t seconds) {
  uint16_t closest = kSleepOptionsSec[0];
  int32_t closest_index = 0;
  uint16_t best_diff = (seconds > closest) ? (seconds - closest) : (closest - seconds);
  for (size_t i = 1; i < kSleepOptionsSecCount; ++i) {
    uint16_t option = kSleepOptionsSec[i];
    uint16_t diff = (seconds > option) ? (seconds - option) : (option - seconds);
    if (diff < best_diff) {
      best_diff = diff;
      closest = option;
      closest_index = static_cast<int32_t>(i);
    }
  }
  return closest_index;
}

static void format_sleep_label(char* buf, size_t len, uint16_t seconds) {
  if (seconds <= 60) {
    snprintf(buf, len, "%u s", static_cast<unsigned>(seconds));
  } else {
    snprintf(buf, len, "%u min", static_cast<unsigned>(seconds / 60));
  }
}

static int32_t sleep_slider_max_index() {
  return static_cast<int32_t>(kSleepOptionsSecCount);
}

static bool sleep_index_is_never(int32_t index) {
  return index >= static_cast<int32_t>(kSleepOptionsSecCount);
}

static void format_sleep_popup_value_for_index(char* buf, size_t len, int32_t index) {
  if (sleep_index_is_never(index)) {
    snprintf(buf, len, "%s", tr().sleep_never);
    return;
  }
  char value[16];
  format_sleep_label(value, sizeof(value), sleep_seconds_from_index(index));
  snprintf(buf, len, "%s %s", tr().sleep_after, value);
}

static void update_display_rotate_label() {
  if (!display_rotate_label) return;
  String icon_char = getMdiChar(String("phone-rotate-landscape"));
  lv_label_set_text(display_rotate_label, icon_char.c_str());
  if (display_rotate_sub_label) {
    static char buf[24];
    snprintf(buf, sizeof(buf), "%u%c", static_cast<unsigned>(display_rotation_quarters * 90), 176);
    lv_label_set_text(display_rotate_sub_label, buf);
  }
}

static void sync_display_rotation_state(uint8_t rotation_quarters) {
  display_rotation_quarters = Device::normalizeRotationQuarterTurns(rotation_quarters);
  display_rotated_180 = (display_rotation_quarters == Device::kRotationFlipped);
  display_rotation_mode = display_rotated_180 ? kDisplayRotationFlipped : kDisplayRotationNormal;
  configManager.setRuntimeDisplayRotationQuarters(display_rotation_quarters);
}

static int brightness_pct_from_raw(int raw) {
  if (raw < kSettingsBrightnessRawMin) raw = kSettingsBrightnessRawMin;
  if (raw > kSettingsBrightnessRawMax) raw = kSettingsBrightnessRawMax;
  const int span = kSettingsBrightnessRawMax - kSettingsBrightnessRawMin;
  if (span <= 0) return kSettingsBrightnessPctMax;
  return kSettingsBrightnessPctMin + static_cast<int>((static_cast<long>(raw - kSettingsBrightnessRawMin) * (kSettingsBrightnessPctMax - kSettingsBrightnessPctMin) + (span / 2)) / span);
}

static uint8_t brightness_raw_from_pct(int pct) {
  if (pct < kSettingsBrightnessPctMin) pct = kSettingsBrightnessPctMin;
  if (pct > kSettingsBrightnessPctMax) pct = kSettingsBrightnessPctMax;
  const int span = kSettingsBrightnessRawMax - kSettingsBrightnessRawMin;
  const int pct_span = kSettingsBrightnessPctMax - kSettingsBrightnessPctMin;
  int raw = kSettingsBrightnessRawMin;
  if (pct_span > 0) {
    raw = kSettingsBrightnessRawMin + static_cast<int>((static_cast<long>(pct - kSettingsBrightnessPctMin) * span + (pct_span / 2)) / pct_span);
  }
  if (raw < kSettingsBrightnessRawMin) raw = kSettingsBrightnessRawMin;
  if (raw > kSettingsBrightnessRawMax) raw = kSettingsBrightnessRawMax;
  return static_cast<uint8_t>(raw);
}
static const char* wake_mode_text(uint8_t mode) {
  (void)mode;
  return tr().touch_label;
}

static void update_wake_button(lv_obj_t *main_label, lv_obj_t *sub_label, uint8_t mode) {
  (void)mode;
  if (!main_label) return;
  wake_mode_mains = kWakeModeTouch;
  wake_mode_battery = kWakeModeTouch;
  lv_label_set_text(main_label, wake_mode_text(kWakeModeTouch));
  if (sub_label) {
    lv_label_set_text(sub_label, tr().no_imu_hint);
  }
}

static void format_status_line(char* buf, size_t len, const char* value) {
  snprintf(buf, len, "Status: %s", value ? value : "---");
}

static void format_ssid_line(char* buf, size_t len, const char* value) {
  snprintf(buf, len, "%s: %s", tr().ssid_label, value ? value : "---");
}

static void format_ip_line(char* buf, size_t len, const char* value) {
  snprintf(buf, len, "%s: %s", tr().ip_label, value ? value : "---");
}

void settings_sync_display_rotation(bool rotated) {
  sync_display_rotation_state(rotated ? Device::kRotationFlipped : Device::kRotationDefault);
  displayManager.setRotation(display_rotation_quarters);
  update_display_rotate_label();
  lv_obj_invalidate(lv_scr_act());
  lv_display_t* disp = lv_display_get_default();
  if (disp) {
    lv_refr_now(disp);
  }
  update_settings_tile_summaries();
}

static void on_display_rotate_clicked(lv_event_t *e) {
  (void)e;
  uint8_t next_rotation = displayManager.getRotation();
  if (Device::supportsQuarterTurnRotation()) {
    next_rotation = (next_rotation + 1) & 0x03;
  } else {
    next_rotation = (next_rotation == Device::kRotationFlipped)
                        ? Device::kRotationDefault
                        : Device::kRotationFlipped;
  }
  displayManager.setRotation(next_rotation);
  sync_display_rotation_state(next_rotation);
  const DeviceConfig& cfg = configManager.getConfig();
  configManager.saveDisplaySettings(
      cfg.display_brightness,
      cfg.auto_sleep_enabled,
      cfg.auto_sleep_seconds,
      cfg.auto_sleep_battery_enabled,
      cfg.auto_sleep_battery_seconds,
      display_rotation_mode,
      display_rotated_180,
      display_rotation_quarters,
      wake_mode_mains,
      wake_mode_battery);
  mqttPublishDeviceSettings();
  update_display_rotate_label();
  lv_obj_invalidate(lv_scr_act());
  lv_display_t* disp = lv_display_get_default();
  if (disp) {
    lv_refr_now(disp);
  }
}

static void on_mains_wake_clicked(lv_event_t *e) {
  (void)e;
  update_wake_button(mains_wake_label, mains_wake_sub_label, kWakeModeTouch);
}

static void on_brightness(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);
  int32_t pct = lv_slider_get_value(slider);
  uint8_t raw = brightness_raw_from_pct(pct);

  BoardHAL::setBrightness(raw);

  static char buf[16];
  snprintf(buf, sizeof(buf), "%d%%", (int)pct);
  if (brightness_label) lv_label_set_text(brightness_label, buf);

  if (code == LV_EVENT_RELEASED) {
    const DeviceConfig& cfg = configManager.getConfig();
    configManager.saveDisplaySettings(
        raw,
        cfg.auto_sleep_enabled,
        cfg.auto_sleep_seconds,
        cfg.auto_sleep_battery_enabled,
        cfg.auto_sleep_battery_seconds,
        display_rotation_mode,
        display_rotated_180,
        display_rotation_quarters,
        wake_mode_mains,
        wake_mode_battery);
    mqttPublishDeviceSettings();
    update_settings_tile_summaries();
  }
}

static lv_obj_t *create_settings_column(lv_obj_t *parent, lv_coord_t width_pct,
                                        lv_flex_align_t main_align, lv_flex_align_t cross_align) {
  lv_obj_t *col = lv_obj_create(parent);
  lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(col, LV_PCT(width_pct));
  lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(col, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(col, 0, 0);
  lv_obj_set_style_pad_row(col, kSettingsColRowGap, 0);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, main_align, cross_align, cross_align);
  return col;
}

static lv_obj_t* create_icon_block(lv_obj_t *parent, const char *icon_name, const char *label_text) {
  lv_obj_t *icon = lv_label_create(parent);
  String icon_char = getMdiChar(String(icon_name));
  lv_label_set_text(icon, icon_char.c_str());
  if (FONT_MDI_ICONS) {
    lv_obj_set_style_text_font(icon, FONT_MDI_ICONS, 0);
  }
  lv_obj_set_width(icon, LV_PCT(100));
  lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(icon, lv_color_white(), 0);

  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, label_text);
  lv_obj_set_width(label, LV_PCT(100));
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(label, &ui_font_20, 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_set_style_margin_top(label, 4, 0);
  return label;
}

static void on_settings_back_clicked(lv_event_t *e) {
  (void)e;
  uiManager.switchToTab(0);
}

static void create_settings_back_button(lv_obj_t *parent) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 22, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_grid_cell(btn,
      LV_GRID_ALIGN_STRETCH, 0, 1,
      LV_GRID_ALIGN_STRETCH, 0, 1);

  uint32_t btn_color = 0x2A2A2A;
  style_settings_button(btn, btn_color);

  lv_obj_add_event_cb(btn, on_settings_back_clicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *icon = lv_label_create(btn);
  String icon_char = getMdiChar(String("arrow-left"));
  lv_label_set_text(icon, icon_char.c_str());
  if (FONT_MDI_ICONS) {
    lv_obj_set_style_text_font(icon, FONT_MDI_ICONS, 0);
  }
  lv_obj_set_style_text_color(icon, lv_color_white(), 0);
  lv_obj_center(icon);
}

static void on_sleep_slider(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);
  int32_t index = lv_slider_get_value(slider);

  static char buf[32];
  format_sleep_popup_value_for_index(buf, sizeof(buf), index);
  if (sleep_time_label) lv_label_set_text(sleep_time_label, buf);

  if (code == LV_EVENT_RELEASED) {
    const DeviceConfig& cfg = configManager.getConfig();
    bool enabled = !sleep_index_is_never(index);
    uint16_t seconds = enabled ? sleep_seconds_from_index(index) : cfg.auto_sleep_seconds;
    configManager.saveDisplaySettings(
        cfg.display_brightness,
        enabled,
        seconds,
        cfg.auto_sleep_battery_enabled,
        cfg.auto_sleep_battery_seconds,
        display_rotation_mode,
        display_rotated_180,
        display_rotation_quarters,
        wake_mode_mains,
        wake_mode_battery);
    mqttPublishDeviceSettings();
    update_settings_tile_summaries();
  }
}

// Power Status Update (stub -> no battery on Waveshare)
void settings_update_power_status() {
  // No battery to monitor -> nothing to update
}

static void clear_ap_confirm_timer() {
  if (ap_confirm_timer) {
    lv_timer_del(ap_confirm_timer);
    ap_confirm_timer = nullptr;
  }
}

static void hide_ap_confirm_row() {
  if (ap_confirm_row) {
    lv_obj_add_flag(ap_confirm_row, LV_OBJ_FLAG_HIDDEN);
  }
  clear_ap_confirm_timer();
  ap_mode_confirm_pending = false;
  if (ap_mode_btn) {
    lv_obj_clear_flag(ap_mode_btn, LV_OBJ_FLAG_HIDDEN);
  }
}

static void on_ap_confirm_timeout(lv_timer_t *timer) {
  if (timer) {
    lv_timer_del(timer);
  }
  ap_confirm_timer = nullptr;
  hide_ap_confirm_row();
}

static void on_confirm_yes_clicked(lv_event_t *e) {
  if (g_hotspot_callback) {
    g_hotspot_callback(true);
  }
  settings_update_ap_mode(true);
  ap_mode_click_block_until = millis() + 400;
  hide_ap_confirm_row();
}

static void on_confirm_no_clicked(lv_event_t *e) {
  ap_mode_click_block_until = millis() + 400;
  hide_ap_confirm_row();
}

static void on_wifi_choose_clicked(lv_event_t *e) {
  (void)e;
  wifi_setup_popup_open();
}

static void on_ap_mode_clicked(lv_event_t *e) {
  if (ap_mode_click_block_until != 0 &&
      (int32_t)(millis() - ap_mode_click_block_until) < 0) {
    return;
  }
  lv_obj_t *btn = (lv_obj_t*)lv_event_get_current_target(e);
  if (btn) {
    ap_mode_btn = btn;
    ap_mode_btn_label = lv_obj_get_child(btn, 0);
  }
  if (ap_mode_active) {
    if (g_hotspot_callback) {
      g_hotspot_callback(false);
    }
    return;
  }
  if (ap_confirm_row && !ap_mode_confirm_pending) {
    ap_mode_confirm_pending = true;
    lv_obj_clear_flag(ap_confirm_row, LV_OBJ_FLAG_HIDDEN);
    clear_ap_confirm_timer();
    ap_confirm_timer = lv_timer_create(on_ap_confirm_timeout, 10000, nullptr);
    if (btn) {
      lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (ap_mode_btn) {
      lv_obj_add_flag(ap_mode_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static lv_obj_t *create_settings_card(lv_obj_t *parent, uint8_t col, uint8_t row) {
  lv_obj_t *card = lv_obj_create(parent);
  uint8_t span = (col < GRID_COLS) ? (GRID_COLS - col) : 1;
  lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, col, span, LV_GRID_ALIGN_STRETCH, row, 1);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_border_opa(card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_pad_hor(card, 12, 0);
  lv_obj_set_style_pad_ver(card, 10, 0);
  lv_obj_set_style_pad_row(card, 4, 0);
  lv_obj_set_style_pad_column(card, kSettingsColGap, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return card;
}

static lv_obj_t *create_card_row(lv_obj_t *parent) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return row;
}

static lv_obj_t *create_slider_row(lv_obj_t *parent) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_bottom(row, 2, 0);
  lv_obj_set_style_pad_right(row, 12, 0);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return row;
}

static void style_settings_slider(lv_obj_t *slider) {
  if (!slider) return;
  lv_obj_set_height(slider, kSettingsSliderHeight);
  lv_obj_set_style_width(slider, kSettingsSliderKnobSize, LV_PART_KNOB);
  lv_obj_set_style_height(slider, kSettingsSliderKnobSize, LV_PART_KNOB);
  lv_obj_set_ext_click_area(slider, kSettingsSliderClickPad);
  lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(slider, LV_OPA_20, LV_PART_MAIN);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_KNOB);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
  lv_obj_set_style_border_width(slider, 0, LV_PART_KNOB);
  lv_obj_clear_flag(slider, LV_OBJ_FLAG_SCROLLABLE);
}

struct SettingsTimezoneOption {
  const char* code;
  const char* label_en;
  const char* label_de;
};

static const SettingsTimezoneOption kSettingsTimezoneOptions[] = {
    {"utc", "UTC+0 - UTC", "UTC+0 - UTC"},
    {"london", "UTC+0 / UTC+1 - London", "UTC+0 / UTC+1 - London"},
    {"berlin", "UTC+1 / UTC+2 - Berlin", "UTC+1 / UTC+2 - Berlin"},
    {"athens", "UTC+2 / UTC+3 - Athens", "UTC+2 / UTC+3 - Athen"},
    {"istanbul", "UTC+3 - Istanbul", "UTC+3 - Istanbul"},
    {"moscow", "UTC+3 - Moscow", "UTC+3 - Moskau"},
    {"honolulu", "UTC-10 - Honolulu", "UTC-10 - Honolulu"},
    {"los_angeles", "UTC-8 / UTC-7 - Los Angeles", "UTC-8 / UTC-7 - Los Angeles"},
    {"phoenix", "UTC-7 - Phoenix", "UTC-7 - Phoenix"},
    {"denver", "UTC-7 / UTC-6 - Denver", "UTC-7 / UTC-6 - Denver"},
    {"chicago", "UTC-6 / UTC-5 - Chicago", "UTC-6 / UTC-5 - Chicago"},
    {"new_york", "UTC-5 / UTC-4 - New York", "UTC-5 / UTC-4 - New York"},
    {"buenos_aires", "UTC-3 - Buenos Aires", "UTC-3 - Buenos Aires"},
    {"sao_paulo", "UTC-3 - Sao Paulo", "UTC-3 - Sao Paulo"},
    {"johannesburg", "UTC+2 - Johannesburg", "UTC+2 - Johannesburg"},
    {"nairobi", "UTC+3 - Nairobi", "UTC+3 - Nairobi"},
    {"dubai", "UTC+4 - Dubai", "UTC+4 - Dubai"},
    {"karachi", "UTC+5 - Karachi", "UTC+5 - Karatschi"},
    {"kolkata", "UTC+5:30 - Kolkata", "UTC+5:30 - Kolkata"},
    {"dhaka", "UTC+6 - Dhaka", "UTC+6 - Dhaka"},
    {"bangkok", "UTC+7 - Bangkok", "UTC+7 - Bangkok"},
    {"singapore", "UTC+8 - Singapore", "UTC+8 - Singapur"},
    {"perth", "UTC+8 - Perth", "UTC+8 - Perth"},
    {"tokyo", "UTC+9 - Tokyo", "UTC+9 - Tokio"},
    {"darwin", "UTC+9:30 - Darwin", "UTC+9:30 - Darwin"},
    {"sydney", "UTC+10 / UTC+11 - Sydney", "UTC+10 / UTC+11 - Sydney"},
    {"auckland", "UTC+12 / UTC+13 - Auckland", "UTC+12 / UTC+13 - Auckland"},
};

static String settings_timezone_options_text;

static bool settings_language_is_german() {
  return tr().code && tr().code[0] == 'd';
}

static uint32_t settings_timezone_index(const char* code) {
  const char* selected = (code && code[0]) ? code : "berlin";
  for (uint32_t i = 0; i < sizeof(kSettingsTimezoneOptions) / sizeof(kSettingsTimezoneOptions[0]); ++i) {
    if (strcmp(kSettingsTimezoneOptions[i].code, selected) == 0) return i;
  }
  return 2;  // berlin
}

static const SettingsTimezoneOption& selected_timezone_option(uint32_t index) {
  const uint32_t count = sizeof(kSettingsTimezoneOptions) / sizeof(kSettingsTimezoneOptions[0]);
  if (index >= count) index = 2;
  return kSettingsTimezoneOptions[index];
}

static const char* timezone_label_for_code(const char* code) {
  const SettingsTimezoneOption& opt = selected_timezone_option(settings_timezone_index(code));
  return settings_language_is_german() ? opt.label_de : opt.label_en;
}

static void build_timezone_dropdown_options() {
  settings_timezone_options_text = "";
  settings_timezone_options_text.reserve(900);
  const bool is_de = settings_language_is_german();
  for (uint32_t i = 0; i < sizeof(kSettingsTimezoneOptions) / sizeof(kSettingsTimezoneOptions[0]); ++i) {
    if (i > 0) settings_timezone_options_text += '\n';
    settings_timezone_options_text += is_de ? kSettingsTimezoneOptions[i].label_de
                                            : kSettingsTimezoneOptions[i].label_en;
  }
}

static void style_plain_container(lv_obj_t* obj) {
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(obj, LV_OPA_TRANSP, 0);
  // border_opa=TRANSP macht die Border nur unsichtbar - LVGL zieht ihre
  // Breite (Theme-Default) trotzdem von lv_obj_get_content_width/height ab
  // (siehe lv_obj_get_style_space_left/right in lv_obj_style.h). Ohne dieses
  // Nullen wird verschachtelter Content (z.B. settings_popup_content ->
  // Spacer) unbemerkt schmaler als sein Elternobjekt.
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_outline_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
}

static void style_popup_textarea(lv_obj_t* ta) {
  lv_obj_set_height(ta, 48);
  lv_textarea_set_one_line(ta, true);
  lv_obj_set_style_text_font(ta, &ui_font_20, 0);
  lv_obj_set_style_bg_color(ta, lv_color_hex(0x1E1E1E), 0);
  lv_obj_set_style_text_color(ta, lv_color_white(), 0);
  lv_obj_set_style_radius(ta, 10, 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(0x555555), 0);
  lv_obj_set_style_border_width(ta, 1, 0);
  lv_obj_set_style_border_opa(ta, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(0x378ADD), LV_STATE_FOCUSED);
  lv_obj_set_style_border_width(ta, 2, LV_STATE_FOCUSED);
  lv_obj_set_style_border_color(ta, lv_color_white(), LV_PART_CURSOR | LV_STATE_FOCUSED);
  lv_obj_set_style_border_width(ta, 2, LV_PART_CURSOR | LV_STATE_FOCUSED);
  lv_obj_set_style_border_side(ta, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR | LV_STATE_FOCUSED);
  lv_obj_set_style_border_opa(ta, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
}

static void style_popup_dropdown(lv_obj_t* dd) {
  lv_obj_set_height(dd, 52);
  lv_obj_set_style_text_font(dd, &ui_font_20, LV_PART_MAIN);
  lv_obj_set_style_text_font(dd, &lv_font_montserrat_20, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(dd, lv_color_hex(0x1E1E1E), 0);
  lv_obj_set_style_text_color(dd, lv_color_white(), 0);
  lv_obj_set_style_text_color(dd, lv_color_white(), LV_PART_INDICATOR);
  lv_obj_set_style_radius(dd, 10, 0);
  lv_obj_set_style_border_color(dd, lv_color_hex(0x555555), 0);
  lv_obj_set_style_border_width(dd, 1, 0);
  lv_obj_set_style_border_opa(dd, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_left(dd, 12, 0);
  lv_obj_set_style_pad_right(dd, 36, 0);
}

static void style_popup_dropdown_list(lv_obj_t* list) {
  if (!list) return;
  lv_obj_set_style_bg_color(list, lv_color_hex(0x1E1E1E), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_text_font(list, &ui_font_20, LV_PART_MAIN);
  lv_obj_set_style_text_color(list, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_radius(list, 10, LV_PART_MAIN);
  lv_obj_set_style_border_color(list, lv_color_hex(0x555555), LV_PART_MAIN);
  lv_obj_set_style_border_width(list, 1, LV_PART_MAIN);
  lv_obj_set_style_border_opa(list, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_all(list, 6, LV_PART_MAIN);
  lv_obj_set_style_bg_color(list, lv_color_hex(0x1976D2), LV_PART_SELECTED);
  lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_SELECTED);
  lv_obj_set_style_text_font(list, &ui_font_20, LV_PART_SELECTED);
  lv_obj_set_style_text_color(list, lv_color_white(), LV_PART_SELECTED);
  lv_obj_set_style_radius(list, 6, LV_PART_SELECTED);
  lv_obj_set_style_bg_color(list, lv_color_hex(0x4A4A4A), LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_SCROLLBAR);
}

static void on_popup_dropdown_ready(lv_event_t* e) {
  lv_obj_t* dd = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
  if (!dd) return;
  style_popup_dropdown_list(lv_dropdown_get_list(dd));
}

static lv_obj_t* create_popup_button(lv_obj_t* parent, const char* text, uint32_t color,
                                     lv_event_cb_t cb) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_height(btn, 64);
  style_settings_button(btn, color);
  lv_obj_set_style_border_opa(btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(btn, 14, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &ui_font_20, 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_center(label);
  return btn;
}

static lv_obj_t* create_form_area(lv_obj_t* parent) {
  lv_obj_t* form = lv_obj_create(parent);
  lv_obj_set_width(form, LV_PCT(100));
  lv_obj_set_flex_grow(form, 1);
  lv_obj_set_style_bg_opa(form, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(form, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(form, 0, 0);
  lv_obj_set_style_pad_row(form, 8, 0);
  lv_obj_set_scrollbar_mode(form, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(form, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  return form;
}

static lv_obj_t* create_form_row(lv_obj_t* parent) {
  lv_obj_t* row = lv_obj_create(parent);
  style_plain_container(row);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, 76);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return row;
}

static lv_obj_t* create_display_control_row(lv_obj_t* parent) {
  lv_obj_t* row = create_form_row(parent);
  lv_obj_set_height(row, 84);
  lv_obj_set_style_pad_column(row, 18, 0);
  return row;
}

static lv_obj_t* create_display_row_label(lv_obj_t* parent, const char* text,
                                          lv_coord_t width,
                                          lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(label, width);
  lv_obj_set_style_text_align(label, align, 0);
  lv_obj_set_style_text_font(label, &ui_font_20, 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  return label;
}

static lv_obj_t* create_flex_spacer(lv_obj_t* parent) {
  lv_obj_t* spacer = lv_obj_create(parent);
  style_plain_container(spacer);
  lv_obj_set_width(spacer, 1);
  lv_obj_set_height(spacer, 1);
  lv_obj_set_flex_grow(spacer, 1);
  return spacer;
}

static lv_obj_t* create_field_box(lv_obj_t* parent, const char* label_text) {
  lv_obj_t* box = lv_obj_create(parent);
  style_plain_container(box);
  lv_obj_set_height(box, 76);
  lv_obj_set_flex_grow(box, 1);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(box, 4, 0);

  lv_obj_t* label = lv_label_create(box);
  lv_label_set_text(label, label_text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(label, LV_PCT(100));
  lv_obj_set_style_text_font(label, &ui_font_16, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xC8C8C8), 0);
  return box;
}

// Tastatur erscheint erst, wenn ein Feld antippt wird (statt permanent unten
// zu stehen), und wechselt fuer reine Zahlen-/IP-Felder auf LVGLs eingebautes
// Ziffernblock-Layout statt der vollen QWERTZ-Tastatur.
static void on_popup_textarea_focused(lv_event_t* e) {
  lv_obj_t* ta = static_cast<lv_obj_t*>(lv_event_get_target(e));
  if (!settings_popup_keyboard || !ta) return;
  const bool numeric = lv_event_get_user_data(e) != nullptr;
  lv_keyboard_set_mode(settings_popup_keyboard,
                       numeric ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
  ui_keyboard_set_target(settings_popup_keyboard, ta, settings_popup_active_ta);
  settings_popup_active_ta = ta;
  lv_obj_clear_flag(settings_popup_keyboard, LV_OBJ_FLAG_HIDDEN);
}

// Tippt der Nutzer ausserhalb eines Feldes (auch Checkbox/Save/X-Button:
// per Default click-focusable), feuert LVGL hier DEFOCUSED, bevor der
// eigentliche Klick verarbeitet wird - blendet die Tastatur also zuverlaessig
// aus, ohne dass jeder Button das separat anstossen muesste.
static void on_popup_textarea_defocused(lv_event_t*) {
  if (settings_popup_keyboard) lv_obj_add_flag(settings_popup_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t* create_textarea_field(lv_obj_t* parent, const char* label_text,
                                       const char* value, const char* placeholder,
                                       uint16_t max_len, bool password,
                                       bool numeric = false) {
  lv_obj_t* box = create_field_box(parent, label_text);
  lv_obj_t* ta = lv_textarea_create(box);
  lv_obj_set_width(ta, LV_PCT(100));
  style_popup_textarea(ta);
  lv_textarea_set_max_length(ta, max_len);
  lv_textarea_set_password_mode(ta, password);
  lv_textarea_set_placeholder_text(ta, placeholder ? placeholder : "");
  lv_textarea_set_text(ta, value ? value : "");
  // PRESSED zusaetzlich zu FOCUSED: FOCUSED haengt an LVGLs interner
  // "last_pressed"-Objektverfolgung fuers Klick-Fokus-Tracking und feuert in
  // manchen Abfolgen (z.B. Tastatur-Ausblenden-Taste, die selbst nicht
  // click-focusable ist, direkt gefolgt von einem neuen Feld) nicht
  // zuverlaessig. PRESSED reagiert dagegen immer sofort auf den Touch.
  lv_obj_add_event_cb(ta, on_popup_textarea_focused, LV_EVENT_PRESSED,
                      numeric ? reinterpret_cast<void*>(1) : nullptr);
  lv_obj_add_event_cb(ta, on_popup_textarea_focused, LV_EVENT_FOCUSED,
                      numeric ? reinterpret_cast<void*>(1) : nullptr);
  lv_obj_add_event_cb(ta, on_popup_textarea_defocused, LV_EVENT_DEFOCUSED, nullptr);
  return ta;
}

static lv_obj_t* create_dropdown_field(lv_obj_t* parent, const char* label_text,
                                       const char* options, uint32_t selected) {
  lv_obj_t* box = create_field_box(parent, label_text);
  lv_obj_t* dd = lv_dropdown_create(box);
  lv_obj_set_width(dd, LV_PCT(100));
  style_popup_dropdown(dd);
  lv_dropdown_set_symbol(dd, LV_SYMBOL_DOWN);
  lv_dropdown_set_options(dd, options);
  lv_dropdown_set_selected(dd, selected);
  style_popup_dropdown_list(lv_dropdown_get_list(dd));
  lv_obj_add_event_cb(dd, on_popup_dropdown_ready, LV_EVENT_READY, nullptr);
  return dd;
}

static void set_obj_disabled(lv_obj_t* obj, bool disabled) {
  if (!obj) return;
  if (disabled) {
    lv_obj_add_state(obj, LV_STATE_DISABLED);
  } else {
    lv_obj_remove_state(obj, LV_STATE_DISABLED);
  }
}

static void update_wifi_static_enabled() {
  const bool enabled = wifi_static_cb && lv_obj_has_state(wifi_static_cb, LV_STATE_CHECKED);
  set_obj_disabled(wifi_ip_ta, !enabled);
  set_obj_disabled(wifi_gateway_ta, !enabled);
  set_obj_disabled(wifi_subnet_ta, !enabled);
  set_obj_disabled(wifi_dns_ta, !enabled);
}

static void on_wifi_static_toggled(lv_event_t*) {
  update_wifi_static_enabled();
}

static void on_wifi_show_pass_toggled(lv_event_t*) {
  if (!wifi_pass_ta || !wifi_show_pass_cb) return;
  lv_textarea_set_password_mode(wifi_pass_ta,
                                !lv_obj_has_state(wifi_show_pass_cb, LV_STATE_CHECKED));
}

static void on_mqtt_show_pass_toggled(lv_event_t*) {
  if (!mqtt_pass_ta || !mqtt_show_pass_cb) return;
  lv_textarea_set_password_mode(mqtt_pass_ta,
                                !lv_obj_has_state(mqtt_show_pass_cb, LV_STATE_CHECKED));
}

static void copy_textarea(char* dst, size_t dst_size, lv_obj_t* ta, bool trim_value) {
  if (!dst || dst_size == 0) return;
  String value = ta ? String(lv_textarea_get_text(ta)) : String("");
  if (trim_value) value.trim();
  strncpy(dst, value.c_str(), dst_size - 1);
  dst[dst_size - 1] = '\0';
}

static void trim_trailing_slashes(String& value) {
  value.trim();
  while (value.endsWith("/")) {
    value.remove(value.length() - 1);
  }
}

static void reset_popup_refs() {
  settings_popup_card = nullptr;
  settings_popup_title = nullptr;
  settings_popup_save_btn = nullptr;
  settings_popup_save_label = nullptr;
  settings_popup_content = nullptr;
  settings_popup_keyboard = nullptr;
  settings_popup_active_ta = nullptr;
  settings_popup_restart_pending = false;

  brightness_label = nullptr;
  display_rotate_btn = nullptr;
  display_rotate_label = nullptr;
  display_rotate_sub_label = nullptr;
  sleep_slider = nullptr;
  sleep_time_label = nullptr;
  sleep_label = nullptr;
  ap_mode_btn = nullptr;
  ap_mode_btn_label = nullptr;

  wifi_ssid_ta = nullptr;
  wifi_pass_ta = nullptr;
  wifi_static_cb = nullptr;
  wifi_ip_ta = nullptr;
  wifi_gateway_ta = nullptr;
  wifi_subnet_ta = nullptr;
  wifi_dns_ta = nullptr;
  wifi_show_pass_cb = nullptr;

  mqtt_host_ta = nullptr;
  mqtt_port_ta = nullptr;
  mqtt_user_ta = nullptr;
  mqtt_pass_ta = nullptr;
  mqtt_client_id_ta = nullptr;
  mqtt_base_ta = nullptr;
  mqtt_ha_prefix_ta = nullptr;
  mqtt_show_pass_cb = nullptr;

  locale_language_dd = nullptr;
  locale_timezone_dd = nullptr;
  locale_time_format_dd = nullptr;
  locale_date_format_dd = nullptr;
}

static void close_settings_popup() {
  if (settings_popup_restart_pending) return;
  if (settings_popup_overlay) {
    lv_obj_del(settings_popup_overlay);
  }
  settings_popup_overlay = nullptr;
  reset_popup_refs();
}

static void on_settings_popup_close_clicked(lv_event_t*) {
  close_settings_popup();
}

static void on_settings_restart_timer(lv_timer_t* t) {
  lv_timer_del(t);
  displayManager.setInputEnabled(false);
  BoardHAL::prepareForRestart();
  delay(150);
  ESP.restart();
}

static void mark_popup_restarting() {
  settings_popup_restart_pending = true;
  if (settings_popup_title) lv_label_set_text(settings_popup_title, tr().wifi_saved_restarting);
  if (settings_popup_keyboard) lv_obj_add_flag(settings_popup_keyboard, LV_OBJ_FLAG_HIDDEN);
  if (settings_popup_content) lv_obj_add_flag(settings_popup_content, LV_OBJ_FLAG_HIDDEN);
  if (settings_popup_save_btn) lv_obj_add_flag(settings_popup_save_btn, LV_OBJ_FLAG_HIDDEN);
  lv_timer_t* t = lv_timer_create(on_settings_restart_timer, 1200, nullptr);
  lv_timer_set_repeat_count(t, 1);
}

static void save_wifi_popup() {
  String ssid_text = wifi_ssid_ta ? String(lv_textarea_get_text(wifi_ssid_ta)) : String("");
  ssid_text.trim();
  if (!ssid_text.length()) return;

  DeviceConfig cfg = configManager.getConfig();
  strncpy(cfg.wifi_ssid, ssid_text.c_str(), sizeof(cfg.wifi_ssid) - 1);
  cfg.wifi_ssid[sizeof(cfg.wifi_ssid) - 1] = '\0';
  copy_textarea(cfg.wifi_pass, sizeof(cfg.wifi_pass), wifi_pass_ta, false);
  const bool use_static =
      wifi_static_cb && lv_obj_has_state(wifi_static_cb, LV_STATE_CHECKED);
  if (use_static) {
    copy_textarea(cfg.wifi_static_ip, sizeof(cfg.wifi_static_ip), wifi_ip_ta, true);
    copy_textarea(cfg.wifi_gateway, sizeof(cfg.wifi_gateway), wifi_gateway_ta, true);
    copy_textarea(cfg.wifi_subnet, sizeof(cfg.wifi_subnet), wifi_subnet_ta, true);
    copy_textarea(cfg.wifi_dns, sizeof(cfg.wifi_dns), wifi_dns_ta, true);
  } else {
    cfg.wifi_static_ip[0] = '\0';
    cfg.wifi_gateway[0] = '\0';
    cfg.wifi_subnet[0] = '\0';
    cfg.wifi_dns[0] = '\0';
  }

  if (!configManager.save(cfg)) {
    if (settings_popup_title) lv_label_set_text(settings_popup_title, tr().wifi_save_failed);
    return;
  }
  mark_popup_restarting();
}

static void save_mqtt_popup() {
  DeviceConfig cfg = configManager.getConfig();
  copy_textarea(cfg.mqtt_host, sizeof(cfg.mqtt_host), mqtt_host_ta, true);

  String port_text = mqtt_port_ta ? String(lv_textarea_get_text(mqtt_port_ta)) : String("");
  port_text.trim();
  int port = port_text.length() ? port_text.toInt() : 1883;
  if (port <= 0 || port > 65535) port = 1883;
  cfg.mqtt_port = static_cast<uint16_t>(port);

  copy_textarea(cfg.mqtt_user, sizeof(cfg.mqtt_user), mqtt_user_ta, true);
  copy_textarea(cfg.mqtt_pass, sizeof(cfg.mqtt_pass), mqtt_pass_ta, false);
  copy_textarea(cfg.mqtt_client_id, sizeof(cfg.mqtt_client_id), mqtt_client_id_ta, true);

  String base = mqtt_base_ta ? String(lv_textarea_get_text(mqtt_base_ta)) : String("");
  trim_trailing_slashes(base);
  if (!base.length()) base = "tab5";
  strncpy(cfg.mqtt_base_topic, base.c_str(), sizeof(cfg.mqtt_base_topic) - 1);
  cfg.mqtt_base_topic[sizeof(cfg.mqtt_base_topic) - 1] = '\0';

  String ha_prefix = mqtt_ha_prefix_ta ? String(lv_textarea_get_text(mqtt_ha_prefix_ta)) : String("");
  trim_trailing_slashes(ha_prefix);
  if (!ha_prefix.length()) ha_prefix = "ha/statestream";
  strncpy(cfg.ha_prefix, ha_prefix.c_str(), sizeof(cfg.ha_prefix) - 1);
  cfg.ha_prefix[sizeof(cfg.ha_prefix) - 1] = '\0';

  if (!configManager.save(cfg)) {
    if (settings_popup_title) lv_label_set_text(settings_popup_title, tr().save_failed);
    return;
  }

  mqttPublishDeviceSettings();
  settings_show_mqtt_warning(!configManager.hasMqttConfig());
  update_settings_tile_summaries();
  close_settings_popup();
}

static void save_localization_popup() {
  DeviceConfig cfg = configManager.getConfig();
  const uint32_t language_index = locale_language_dd ? lv_dropdown_get_selected(locale_language_dd) : 0;
  const char* language = (language_index == 1) ? "de" : "en";
  strncpy(cfg.language, i18n::normalize_language_code(language), sizeof(cfg.language) - 1);
  cfg.language[sizeof(cfg.language) - 1] = '\0';

  const uint32_t timezone_index = locale_timezone_dd ? lv_dropdown_get_selected(locale_timezone_dd) : 2;
  const SettingsTimezoneOption& tz = selected_timezone_option(timezone_index);
  strncpy(cfg.timezone, tz.code, sizeof(cfg.timezone) - 1);
  cfg.timezone[sizeof(cfg.timezone) - 1] = '\0';

  cfg.global_time_format = clock_tile::normalize_time_format(
      locale_time_format_dd ? static_cast<int>(lv_dropdown_get_selected(locale_time_format_dd)) : 0);
  cfg.global_date_format = clock_tile::normalize_date_format(
      locale_date_format_dd ? static_cast<int>(lv_dropdown_get_selected(locale_date_format_dd)) : 0);

  if (!configManager.save(cfg)) {
    if (settings_popup_title) lv_label_set_text(settings_popup_title, tr().save_failed);
    return;
  }

  settings_refresh_language();
  uiManager.scheduleNtpSync(0);
  tiles_request_reload_all();
  close_settings_popup();
}

static void save_settings_popup() {
  switch (settings_popup_kind) {
    case SettingsPopupKind::Wifi:
      save_wifi_popup();
      break;
    case SettingsPopupKind::Mqtt:
      save_mqtt_popup();
      break;
    case SettingsPopupKind::Localization:
      save_localization_popup();
      break;
    default:
      break;
  }
}

static void on_settings_popup_save_clicked(lv_event_t*) {
  save_settings_popup();
}

static void hide_popup_keyboard() {
  if (settings_popup_active_ta) {
    lv_obj_remove_state(settings_popup_active_ta, LV_STATE_FOCUSED);
    settings_popup_active_ta = nullptr;
  }
  if (settings_popup_keyboard) lv_obj_add_flag(settings_popup_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void on_popup_keyboard_event(lv_event_t* e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    save_settings_popup();
  } else if (code == LV_EVENT_CANCEL) {
    // Die kleine Tastatur-Taste unten links soll nur die Tastatur einklappen,
    // nicht das ganze Popup schliessen (frueher: close_settings_popup(), was
    // ungesicherte Aenderungen verworfen hat).
    hide_popup_keyboard();
  }
}

static void on_popup_ap_mode_clicked(lv_event_t*) {
  if (ap_mode_click_block_until != 0 &&
      (int32_t)(millis() - ap_mode_click_block_until) < 0) {
    return;
  }
  if (g_hotspot_callback) {
    g_hotspot_callback(!ap_mode_active);
  }
  settings_update_ap_mode(!ap_mode_active);
  ap_mode_click_block_until = millis() + 400;
}

static void build_display_popup(lv_obj_t* parent) {
  const DeviceConfig& cfg = configManager.getConfig();
  lv_obj_t* form = create_form_area(parent);
  lv_obj_clear_flag(form, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* brightness_row = create_display_control_row(form);
  brightness_title_label =
      create_display_row_label(brightness_row, tr().brightness_label, 160);

  lv_obj_t* brightness_slider = lv_slider_create(brightness_row);
  style_settings_slider(brightness_slider);
  lv_obj_set_width(brightness_slider, 1);
  lv_obj_set_flex_grow(brightness_slider, 1);
  lv_slider_set_range(brightness_slider, kSettingsBrightnessPctMin, kSettingsBrightnessPctMax);
  const int current_brightness_pct = brightness_pct_from_raw(BoardHAL::getBrightness());
  lv_slider_set_value(brightness_slider, current_brightness_pct, LV_ANIM_OFF);
  lv_obj_add_event_cb(brightness_slider, on_brightness, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(brightness_slider, on_brightness, LV_EVENT_RELEASED, nullptr);

  static char bright_buf[16];
  snprintf(bright_buf, sizeof(bright_buf), "%d%%", current_brightness_pct);
  brightness_label =
      create_display_row_label(brightness_row, bright_buf, 112, LV_TEXT_ALIGN_RIGHT);

  lv_obj_t* sleep_row = create_display_control_row(form);
  sleep_label = create_display_row_label(sleep_row, tr().sleep_label, 160);

  sleep_slider = lv_slider_create(sleep_row);
  style_settings_slider(sleep_slider);
  lv_obj_set_width(sleep_slider, 1);
  lv_obj_set_flex_grow(sleep_slider, 1);
  lv_slider_set_range(sleep_slider, 0, sleep_slider_max_index());
  int32_t sleep_index = cfg.auto_sleep_enabled
                            ? sleep_index_from_seconds(cfg.auto_sleep_seconds)
                            : sleep_slider_max_index();
  lv_slider_set_value(sleep_slider, sleep_index, LV_ANIM_OFF);
  lv_obj_add_event_cb(sleep_slider, on_sleep_slider, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(sleep_slider, on_sleep_slider, LV_EVENT_RELEASED, nullptr);

  static char sleep_buf[32];
  format_sleep_popup_value_for_index(sleep_buf, sizeof(sleep_buf), sleep_index);
  sleep_time_label =
      create_display_row_label(sleep_row, sleep_buf, 128, LV_TEXT_ALIGN_RIGHT);

  lv_obj_t* rotate_row = create_display_control_row(form);
  create_display_row_label(rotate_row, "Rotation", 160);
  create_flex_spacer(rotate_row);

  display_rotate_btn = create_popup_button(rotate_row, "", 0x2E7D32, on_display_rotate_clicked);
  lv_obj_set_width(display_rotate_btn, LV_PCT(42));
  lv_obj_set_height(display_rotate_btn, 64);
  display_rotate_label = lv_obj_get_child(display_rotate_btn, 0);
  if (FONT_MDI_ICONS) lv_obj_set_style_text_font(display_rotate_label, FONT_MDI_ICONS, 0);
  update_display_rotate_label();
}

// Tastatur bleibt naeher am Card-Rand als das restliche Formular: statt der
// normalen 20px Card-Padding nur kKeyboardInset auf allen drei Seiten
// (links/rechts/unten). Dafuer wird sie direkt in settings_popup_card
// (ignore-layout, wie Save-/Close-Button) statt in settings_popup_content
// gehaengt - andernfalls wuerde settings_popup_content (pad=0) das Ausbrechen
// aus seiner eigenen Box abschneiden.
static constexpr int kPopupCardPad = 20;  // muss zu settings_popup_card passen
static constexpr int kKeyboardInset = 11;
static constexpr int kKeyboardBleed = kPopupCardPad - kKeyboardInset;

// content_parent = settings_popup_content (Breite/Hoehe zur Laufzeit
// gemessen statt aus SCREEN_WIDTH/HEIGHT hochgerechnet - Letzteres passte
// nicht zur tatsaechlichen Kartenbreite und liess rechts keinen Rand).
static void create_popup_keyboard(lv_obj_t* content_parent) {
  // Platzhalter in der normalen Flex-Spalte (wie "form" ein Kind von
  // content_parent): sorgt dafuer, dass "form" wie vorher schrumpft und die
  // unteren Feld-/Button-Reihen nicht von der jetzt frei positionierten
  // Tastatur verdeckt werden.
  lv_obj_t* spacer = lv_obj_create(content_parent);
  style_plain_container(spacer);
  lv_obj_clear_flag(spacer, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_width(spacer, LV_PCT(100));
  lv_obj_set_height(spacer, LV_PCT(42));
  lv_obj_update_layout(content_parent);
  const int reserved_w = lv_obj_get_width(spacer);
  const int reserved_h = lv_obj_get_height(spacer);

  lv_obj_t* kb = ui_keyboard_create(settings_popup_card);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_IGNORE_LAYOUT);
  const int kb_w = reserved_w + (kKeyboardBleed * 2);
  const int kb_h = reserved_h + kKeyboardBleed;
  lv_obj_set_size(kb, kb_w, kb_h);
  // LVGL rechnet bei lv_obj_align IMMER das Padding des Elternobjekts (card)
  // mit ein - auch bei TOP_LEFT (lv_obj_move_to addiert space_left/top des
  // Parents unconditional). BOTTOM_LEFT mit negativem Offset ist also der
  // richtige Weg: card_pad(20) + (-bleed) = Inset links, card_pad(20) - bleed
  // via ph-h+bleed = Inset unten, Breite (siehe reserved_w) sorgt fuer Inset
  // rechts - vorausgesetzt style_plain_container nullt auch border_width
  // (nicht nur border_opa), sonst zaehlt LVGLs unsichtbare Theme-Border
  // zusaetzlich zum Padding und macht reserved_w schmaler als die Card
  // tatsaechlich ist (siehe style_plain_container-Kommentar).
  lv_obj_align(kb, LV_ALIGN_BOTTOM_LEFT, -kKeyboardBleed, kKeyboardBleed);
  lv_obj_add_event_cb(kb, on_popup_keyboard_event, LV_EVENT_ALL, nullptr);

  // Erst sichtbar, sobald ein Feld fokussiert wird (on_popup_textarea_focused) -
  // kein Auto-Fokus eines Feldes beim Oeffnen mehr.
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  settings_popup_keyboard = kb;
}

static void build_wifi_popup(lv_obj_t* parent) {
  const DeviceConfig& cfg = configManager.getConfig();
  lv_obj_t* form = create_form_area(parent);

  lv_obj_t* row1 = create_form_row(form);
  wifi_ssid_ta = create_textarea_field(row1, tr().ssid_label, cfg.wifi_ssid,
                                       tr().ap_wifi_ssid_placeholder,
                                       CONFIG_WIFI_SSID_MAX - 1, false);
  wifi_pass_ta = create_textarea_field(row1, tr().wifi_password_label, cfg.wifi_pass,
                                       tr().wifi_password_label,
                                       CONFIG_WIFI_PASS_MAX - 1, true);

  wifi_show_pass_cb = lv_checkbox_create(form);
  lv_checkbox_set_text(wifi_show_pass_cb, tr().wifi_show_password);
  lv_obj_set_style_text_font(wifi_show_pass_cb, &ui_font_20, 0);
  lv_obj_set_style_text_color(wifi_show_pass_cb, lv_color_hex(0xC8C8C8), 0);
  lv_obj_add_event_cb(wifi_show_pass_cb, on_wifi_show_pass_toggled, LV_EVENT_VALUE_CHANGED, nullptr);

  wifi_static_cb = lv_checkbox_create(form);
  lv_checkbox_set_text(wifi_static_cb, tr().wifi_static_ip_label);
  lv_obj_set_style_text_font(wifi_static_cb, &ui_font_20, 0);
  lv_obj_set_style_text_color(wifi_static_cb, lv_color_hex(0xC8C8C8), 0);
  const bool has_static = cfg.wifi_static_ip[0] || cfg.wifi_gateway[0] ||
                          cfg.wifi_subnet[0] || cfg.wifi_dns[0];
  if (has_static) lv_obj_add_state(wifi_static_cb, LV_STATE_CHECKED);
  lv_obj_add_event_cb(wifi_static_cb, on_wifi_static_toggled, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t* row2 = create_form_row(form);
  wifi_ip_ta = create_textarea_field(row2, tr().wifi_static_ip_label, cfg.wifi_static_ip,
                                     "192.168.1.50", CONFIG_IP_ADDR_MAX - 1, false, true);
  wifi_gateway_ta = create_textarea_field(row2, tr().wifi_gateway_label, cfg.wifi_gateway,
                                          "192.168.1.1", CONFIG_IP_ADDR_MAX - 1, false, true);

  lv_obj_t* row3 = create_form_row(form);
  wifi_subnet_ta = create_textarea_field(row3, tr().wifi_subnet_label, cfg.wifi_subnet,
                                         "255.255.255.0", CONFIG_IP_ADDR_MAX - 1, false, true);
  wifi_dns_ta = create_textarea_field(row3, tr().wifi_dns_label, cfg.wifi_dns,
                                      "192.168.1.1", CONFIG_IP_ADDR_MAX - 1, false, true);

  lv_obj_t* hint = lv_label_create(form);
  lv_label_set_text(hint, tr().wifi_dhcp_hint);
  lv_obj_set_style_text_font(hint, &ui_font_16, 0);
  lv_obj_set_style_text_color(hint, lv_color_hex(0xA0A0A0), 0);

  ap_mode_btn = create_popup_button(form, ap_mode_active ? tr().ap_disable : tr().ap_enable,
                                    ap_mode_active ? 0xC62828 : 0xFF9800,
                                    on_popup_ap_mode_clicked);
  lv_obj_set_width(ap_mode_btn, LV_PCT(100));
  ap_mode_btn_label = lv_obj_get_child(ap_mode_btn, 0);
  update_wifi_static_enabled();

  create_popup_keyboard(parent);
}

static void build_mqtt_popup(lv_obj_t* parent) {
  const DeviceConfig& cfg = configManager.getConfig();
  lv_obj_t* form = create_form_area(parent);

  char port_buf[8];
  snprintf(port_buf, sizeof(port_buf), "%u", cfg.mqtt_port ? cfg.mqtt_port : 1883);

  lv_obj_t* row1 = create_form_row(form);
  mqtt_host_ta = create_textarea_field(row1, tr().mqtt_host, cfg.mqtt_host,
                                       "192.168.1.10", CONFIG_MQTT_HOST_MAX - 1, false);
  mqtt_port_ta = create_textarea_field(row1, tr().mqtt_port, port_buf,
                                       "1883", 5, false, true);

  lv_obj_t* row2 = create_form_row(form);
  mqtt_user_ta = create_textarea_field(row2, tr().mqtt_username, cfg.mqtt_user,
                                       tr().mqtt_username, CONFIG_MQTT_USER_MAX - 1, false);
  mqtt_pass_ta = create_textarea_field(row2, tr().mqtt_password, cfg.mqtt_pass,
                                       tr().mqtt_password, CONFIG_MQTT_PASS_MAX - 1, true);

  mqtt_show_pass_cb = lv_checkbox_create(form);
  lv_checkbox_set_text(mqtt_show_pass_cb, tr().wifi_show_password);
  lv_obj_set_style_text_font(mqtt_show_pass_cb, &ui_font_20, 0);
  lv_obj_set_style_text_color(mqtt_show_pass_cb, lv_color_hex(0xC8C8C8), 0);
  lv_obj_add_event_cb(mqtt_show_pass_cb, on_mqtt_show_pass_toggled, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t* row3 = create_form_row(form);
  mqtt_client_id_ta = create_textarea_field(row3, tr().mqtt_client_id, cfg.mqtt_client_id,
                                            tr().mqtt_client_id_placeholder,
                                            CONFIG_MQTT_CLIENT_ID_MAX - 1, false);

  lv_obj_t* row4 = create_form_row(form);
  mqtt_base_ta = create_textarea_field(row4, tr().mqtt_base_topic,
                                       cfg.mqtt_base_topic[0] ? cfg.mqtt_base_topic : "tab5",
                                       "tab5", CONFIG_MQTT_BASE_MAX - 1, false);
  mqtt_ha_prefix_ta = create_textarea_field(row4, tr().ha_prefix,
                                            cfg.ha_prefix[0] ? cfg.ha_prefix : "ha/statestream",
                                            "ha/statestream", CONFIG_HA_PREFIX_MAX - 1, false);

  lv_obj_t* hint = lv_label_create(form);
  lv_label_set_text(hint, tr().mqtt_client_id_hint);
  lv_obj_set_style_text_font(hint, &ui_font_16, 0);
  lv_obj_set_style_text_color(hint, lv_color_hex(0xA0A0A0), 0);

  create_popup_keyboard(parent);
}

static String format_options_text(bool time_format) {
  String opts;
  opts.reserve(80);
  opts += tr().format_auto_language;
  opts += '\n';
  if (time_format) {
    opts += tr().format_24_hour;
    opts += '\n';
    opts += tr().format_12_hour;
  } else {
    opts += "DD.MM.YYYY\nMM/DD/YYYY\nYYYY/MM/DD";
  }
  return opts;
}

static void build_localization_popup(lv_obj_t* parent) {
  const DeviceConfig& cfg = configManager.getConfig();
  lv_obj_t* form = create_form_area(parent);
  lv_obj_clear_flag(form, LV_OBJ_FLAG_SCROLLABLE);

  build_timezone_dropdown_options();
  const bool language_is_de = i18n::normalize_language_code(cfg.language)[0] == 'd';
  const String time_options = format_options_text(true);
  const String date_options = format_options_text(false);

  lv_obj_t* row1 = create_form_row(form);
  locale_language_dd = create_dropdown_field(row1, tr().language_label,
                                             "English\nDeutsch", language_is_de ? 1 : 0);
  locale_timezone_dd = create_dropdown_field(row1, tr().timezone_label,
                                             settings_timezone_options_text.c_str(),
                                             settings_timezone_index(cfg.timezone));

  lv_obj_t* row2 = create_form_row(form);
  locale_time_format_dd = create_dropdown_field(row2, tr().time_format_label,
                                                time_options.c_str(),
                                                clock_tile::normalize_time_format(cfg.global_time_format));
  locale_date_format_dd = create_dropdown_field(row2, tr().date_format_label,
                                                date_options.c_str(),
                                                clock_tile::normalize_date_format(cfg.global_date_format));
}

static void build_firmware_popup(lv_obj_t* parent) {
  lv_obj_t* box = lv_obj_create(parent);
  style_plain_container(box);
  lv_obj_set_width(box, LV_PCT(100));
  lv_obj_set_flex_grow(box, 1);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(box, 18, 0);

  lv_obj_t* icon = lv_label_create(box);
  lv_label_set_text(icon, getMdiChar("chip").c_str());
  if (FONT_MDI_ICONS) lv_obj_set_style_text_font(icon, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(icon, lv_color_white(), 0);

  lv_obj_t* label = lv_label_create(box);
  lv_label_set_text(label, "Version");
  lv_obj_set_style_text_font(label, &ui_font_24, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xC8C8C8), 0);

  lv_obj_t* version = lv_label_create(box);
  lv_label_set_text(version, FW_VERSION);
  lv_obj_set_style_text_font(version, &ui_font_32, 0);
  lv_obj_set_style_text_color(version, lv_color_white(), 0);
}

static const char* popup_title_for_kind(SettingsPopupKind kind) {
  switch (kind) {
    case SettingsPopupKind::Display:
      return tr().display_label;
    case SettingsPopupKind::Wifi:
      return tr().wifi_label;
    case SettingsPopupKind::Mqtt:
      return tr().admin_settings_mqtt;
    case SettingsPopupKind::Localization:
      return tr().admin_settings_language;
    case SettingsPopupKind::Firmware:
      return "Firmware";
  }
  return "";
}

static const char* popup_icon_for_kind(SettingsPopupKind kind) {
  switch (kind) {
    case SettingsPopupKind::Display:
      return "monitor";
    case SettingsPopupKind::Wifi:
      return "wifi";
    case SettingsPopupKind::Mqtt:
      return "server";
    case SettingsPopupKind::Localization:
      return "translate";
    case SettingsPopupKind::Firmware:
      return "chip";
  }
  return "cog";
}

static void build_popup_content(SettingsPopupKind kind, lv_obj_t* parent) {
  switch (kind) {
    case SettingsPopupKind::Display:
      build_display_popup(parent);
      break;
    case SettingsPopupKind::Wifi:
      build_wifi_popup(parent);
      break;
    case SettingsPopupKind::Mqtt:
      build_mqtt_popup(parent);
      break;
    case SettingsPopupKind::Localization:
      build_localization_popup(parent);
      break;
    case SettingsPopupKind::Firmware:
      build_firmware_popup(parent);
      break;
  }
}

static bool popup_kind_has_save(SettingsPopupKind kind) {
  return kind == SettingsPopupKind::Wifi ||
         kind == SettingsPopupKind::Mqtt ||
         kind == SettingsPopupKind::Localization;
}

static void open_settings_popup(SettingsPopupKind kind) {
  if (settings_popup_overlay) return;
  reset_popup_refs();
  settings_popup_kind = kind;

  settings_popup_overlay = lv_obj_create(lv_screen_active());
  lv_obj_set_size(settings_popup_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(settings_popup_overlay, 0, 0);
  lv_obj_set_style_bg_color(settings_popup_overlay, lv_color_hex(0x0A0A0A), 0);
  lv_obj_set_style_bg_opa(settings_popup_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_opa(settings_popup_overlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(settings_popup_overlay, 0, 0);
  lv_obj_set_style_radius(settings_popup_overlay, 0, 0);
  lv_obj_set_style_pad_all(settings_popup_overlay, Device::kGridPad, 0);
  lv_obj_clear_flag(settings_popup_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(settings_popup_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(settings_popup_overlay, LV_OBJ_FLAG_FLOATING);

  settings_popup_card = lv_obj_create(settings_popup_overlay);
  lv_obj_set_size(settings_popup_card, LV_PCT(100), LV_PCT(100));
  lv_obj_center(settings_popup_card);
  lv_obj_set_style_bg_color(settings_popup_card, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_border_opa(settings_popup_card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(settings_popup_card, 0, 0);
  lv_obj_set_style_radius(settings_popup_card, 22, 0);
  lv_obj_set_style_clip_corner(settings_popup_card, false, 0);
  lv_obj_set_style_pad_all(settings_popup_card, 20, 0);
  lv_obj_set_style_pad_row(settings_popup_card, 8, 0);
  lv_obj_clear_flag(settings_popup_card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(settings_popup_card, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* header = lv_obj_create(settings_popup_card);
  lv_obj_set_width(header, LV_PCT(100));
  // Hoehe so gewaehlt, dass LEFT_MID-Kinder (Icon/Titel) exakt auf der
  // Mittelachse der Speichern/X-Buttons landen (beide Buttons zentrieren
  // sich bei content-y=42; siehe deren align-Offsets weiter unten).
  lv_obj_set_height(header, 84);
  style_plain_container(header);

  lv_obj_t* header_icon = lv_label_create(header);
  lv_label_set_text(header_icon, getMdiChar(popup_icon_for_kind(kind)).c_str());
  if (FONT_MDI_ICONS) lv_obj_set_style_text_font(header_icon, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(header_icon, lv_color_white(), 0);
  lv_obj_align(header_icon, LV_ALIGN_LEFT_MID, 8, 0);

  settings_popup_title = lv_label_create(header);
  lv_label_set_text(settings_popup_title, popup_title_for_kind(kind));
  lv_label_set_long_mode(settings_popup_title, LV_LABEL_LONG_DOT);
  lv_obj_set_width(settings_popup_title, popup_kind_has_save(kind) ? LV_PCT(46) : LV_PCT(62));
  lv_obj_set_style_text_font(settings_popup_title, &ui_font_24, 0);
  lv_obj_set_style_text_color(settings_popup_title, lv_color_white(), 0);
  lv_obj_align(settings_popup_title, LV_ALIGN_LEFT_MID, 78, 0);

  settings_popup_save_btn = create_popup_button(settings_popup_card, tr().save, 0x2E7D32,
                                                on_settings_popup_save_clicked);
  lv_obj_add_flag(settings_popup_save_btn, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_width(settings_popup_save_btn, 150);
  lv_obj_align(settings_popup_save_btn, LV_ALIGN_TOP_RIGHT, -108, 10);
  settings_popup_save_label = lv_obj_get_child(settings_popup_save_btn, 0);
  if (!popup_kind_has_save(kind)) {
    lv_obj_add_flag(settings_popup_save_btn, LV_OBJ_FLAG_HIDDEN);
  }

  lv_obj_t* close_btn = lv_button_create(settings_popup_card);
  lv_obj_add_flag(close_btn, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(close_btn, 96, 96);
  lv_obj_set_style_bg_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(close_btn, LV_OPA_20, LV_STATE_PRESSED);
  lv_obj_set_style_border_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(close_btn, 16, 0);
  lv_obj_set_style_pad_all(close_btn, 0, 0);
  lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, 6, -6);
  lv_obj_set_ext_click_area(close_btn, 8);
  lv_obj_add_flag(close_btn, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_clear_flag(close_btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(close_btn, on_settings_popup_close_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* close_label = lv_label_create(close_btn);
  lv_obj_set_style_text_font(close_label, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(close_label, lv_color_white(), 0);
  lv_label_set_text(close_label, getMdiChar("window-close").c_str());
  lv_obj_center(close_label);

  settings_popup_content = lv_obj_create(settings_popup_card);
  lv_obj_set_width(settings_popup_content, LV_PCT(100));
  lv_obj_set_flex_grow(settings_popup_content, 1);
  style_plain_container(settings_popup_content);
  lv_obj_set_style_pad_all(settings_popup_content, 0, 0);
  lv_obj_set_style_pad_row(settings_popup_content, 8, 0);
  lv_obj_set_flex_flow(settings_popup_content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(settings_popup_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  build_popup_content(settings_popup_kind, settings_popup_content);

  lv_obj_move_foreground(settings_popup_overlay);
  lv_obj_invalidate(settings_popup_overlay);

  if (lv_display_t* disp = lv_display_get_default()) {
    lv_refr_now(disp);
  }
}

static void on_settings_tile_clicked(lv_event_t* e) {
  const uintptr_t raw = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  open_settings_popup(static_cast<SettingsPopupKind>(static_cast<uint8_t>(raw)));
}

static lv_obj_t* create_settings_menu_tile(lv_obj_t* parent, uint8_t col, uint8_t row,
                                           const char* icon_name, const char* title,
                                           lv_obj_t** title_label_out,
                                           lv_obj_t** summary_label_out,
                                           SettingsPopupKind kind,
                                           lv_obj_t** info_container_out = nullptr) {
  lv_obj_t* tile = lv_button_create(parent);
  lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_grid_cell(tile, LV_GRID_ALIGN_STRETCH, col, 2, LV_GRID_ALIGN_STRETCH, row, 1);
  style_settings_button(tile, 0x2A2A2A);
  lv_obj_set_style_radius(tile, 22, 0);
  lv_obj_set_style_border_opa(tile, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(tile, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(tile, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(tile, 0, 0);
  lv_obj_set_style_pad_column(tile, 0, 0);
  lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_event_cb(tile, on_settings_tile_clicked, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(
                          static_cast<uintptr_t>(static_cast<uint8_t>(kind))));

  lv_obj_t* face = lv_obj_create(tile);
  style_plain_container(face);
  lv_obj_clear_flag(face, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(face, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_set_width(face, GRID_CELL_W);
  lv_obj_set_height(face, LV_PCT(100));
  lv_obj_set_style_pad_all(face, 0, 0);

  lv_obj_t* icon = lv_label_create(face);
  lv_label_set_text(icon, getMdiChar(icon_name).c_str());
  if (FONT_MDI_ICONS) lv_obj_set_style_text_font(icon, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(icon, lv_color_white(), 0);
  lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(icon, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_align(icon, LV_ALIGN_CENTER, 0, -20);

  lv_obj_t* title_label = lv_label_create(face);
  lv_label_set_text(title_label, title);
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(title_label, GRID_CELL_W - 20);
  lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(title_label, &ui_font_20, 0);
  lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
  lv_obj_clear_flag(title_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(title_label, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 35);
  if (title_label_out) *title_label_out = title_label;

  lv_obj_t* info = lv_obj_create(tile);
  style_plain_container(info);
  lv_obj_clear_flag(info, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(info, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_set_height(info, LV_PCT(100));
  lv_obj_set_flex_grow(info, 1);
  lv_obj_set_style_pad_hor(info, 12, 0);
  lv_obj_set_style_pad_ver(info, 8, 0);
  lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(info, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(info, 3, 0);
  if (info_container_out) *info_container_out = info;

  if (summary_label_out) {
    lv_obj_t* summary = lv_label_create(info);
    lv_label_set_text(summary, "");
    lv_label_set_long_mode(summary, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(summary, LV_PCT(100));
    lv_obj_set_style_text_align(summary, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(summary, &ui_font_16, 0);
    lv_obj_set_style_text_color(summary, lv_color_hex(0xA8A8A8), 0);
    *summary_label_out = summary;
  }
  return tile;
}

static void update_settings_tile_summaries() {
  const DeviceConfig& cfg = configManager.getConfig();
  static char buf[128];

  if (settings_tile_display_summary) {
    char sleep_buf[16];
    if (cfg.auto_sleep_enabled) {
      format_sleep_label(sleep_buf, sizeof(sleep_buf), cfg.auto_sleep_seconds);
    } else {
      snprintf(sleep_buf, sizeof(sleep_buf), "%s", tr().sleep_never);
    }
    snprintf(buf, sizeof(buf), "%d%% / %s", brightness_pct_from_raw(BoardHAL::getBrightness()), sleep_buf);
    lv_label_set_text(settings_tile_display_summary, buf);
  }

  if (settings_tile_mqtt_summary) {
    const bool mqtt_configured = cfg.mqtt_host[0] != '\0';
    if (mqtt_configured) {
      snprintf(buf, sizeof(buf), "%s:%u", cfg.mqtt_host, cfg.mqtt_port ? cfg.mqtt_port : 1883);
    } else {
      snprintf(buf, sizeof(buf), "%s", tr().mqtt_not_configured);
    }
    lv_label_set_text(settings_tile_mqtt_summary, buf);
    lv_obj_set_style_text_color(settings_tile_mqtt_summary,
                                lv_color_hex(mqtt_configured ? 0xA8A8A8 : 0xFFC04D),
                                0);
  }

  if (settings_tile_locale_summary) {
    const char* language = (i18n::normalize_language_code(cfg.language)[0] == 'd')
                               ? tr().language_option_german
                               : tr().language_option_english;
    snprintf(buf, sizeof(buf), "%s / %s", language, timezone_label_for_code(cfg.timezone));
    lv_label_set_text(settings_tile_locale_summary, buf);
  }

  if (settings_tile_firmware_summary) {
    lv_label_set_text(settings_tile_firmware_summary, FW_VERSION);
  }
}

static void build_grid_track_descriptors(lv_coord_t* dsc, uint8_t count, lv_coord_t cell_size) {
  if (!dsc) return;
  for (uint8_t i = 0; i < count; ++i) {
    dsc[i] = cell_size;
  }
  dsc[count] = LV_GRID_TEMPLATE_LAST;
}

static void next_settings_tile_cell(uint8_t& col, uint8_t& row,
                                    uint8_t span_w,
                                    uint8_t& out_col, uint8_t& out_row) {
  if (span_w == 0) span_w = 1;
  if (span_w > GRID_COLS) span_w = GRID_COLS;
  if (col + span_w > GRID_COLS) {
    col = 0;
    row++;
  }
  out_col = col;
  out_row = row;
  col += span_w;
}

// ========== Public API ==========
void build_settings_tab(lv_obj_t *tab, hotspot_callback_t hotspot_cb) {
  g_hotspot_callback = hotspot_cb;
  ap_mode_confirm_pending = false;
  clear_ap_confirm_timer();
  if (settings_popup_overlay) close_settings_popup();

  lv_obj_clean(tab);
  lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(tab, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
  lv_obj_set_style_border_opa(tab, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(tab, GRID_PAD, 0);

  // 4x4 Grid
  static lv_coord_t col_dsc[GRID_COLS + 1];
  static lv_coord_t row_dsc[GRID_ROWS + 1];
  static bool dsc_ready = false;
  if (!dsc_ready) {
    build_grid_track_descriptors(col_dsc, GRID_COLS, GRID_CELL_W);
    build_grid_track_descriptors(row_dsc, GRID_ROWS, GRID_CELL_H);
    dsc_ready = true;
  }
  lv_obj_set_layout(tab, LV_LAYOUT_GRID);
  lv_obj_set_grid_dsc_array(tab, col_dsc, row_dsc);
  lv_obj_set_style_pad_column(tab, GRID_GAP, 0);
  lv_obj_set_style_pad_row(tab, GRID_GAP, 0);

  const DeviceConfig& cfg = configManager.getConfig();
  display_rotated_180 = cfg.display_rotated_180;
  display_rotation_quarters = Device::normalizeRotationQuarterTurns(cfg.display_rotation_quarters);
  display_rotation_mode = cfg.display_rotation_mode;
  wake_mode_mains = kWakeModeTouch;
  wake_mode_battery = kWakeModeTouch;

  display_section_label = nullptr;
  brightness_title_label = nullptr;
  wifi_section_label = nullptr;
  sleep_section_label = nullptr;
  sleep_label = nullptr;
  brightness_label = nullptr;
  display_rotate_btn = nullptr;
  display_rotate_label = nullptr;
  display_rotate_sub_label = nullptr;
  sleep_slider = nullptr;
  sleep_time_label = nullptr;
  wifi_choose_btn = nullptr;
  wifi_choose_btn_label = nullptr;
  ap_mode_btn = nullptr;
  ap_mode_btn_label = nullptr;
  ap_confirm_row = nullptr;
  ap_confirm_yes_btn = nullptr;
  ap_confirm_no_btn = nullptr;
  ap_yes_label_obj = nullptr;
  ap_no_label_obj = nullptr;

  create_settings_back_button(tab);

  static constexpr uint8_t kSettingsMenuTileSpanW = 2;
  uint8_t settings_col = 0;
  uint8_t settings_row = 1;
  uint8_t tile_col = 0;
  uint8_t tile_row = 1;

  next_settings_tile_cell(settings_col, settings_row, kSettingsMenuTileSpanW, tile_col, tile_row);
  create_settings_menu_tile(tab, tile_col, tile_row, "monitor", tr().display_label,
                            &settings_tile_display_title,
                            &settings_tile_display_summary,
                            SettingsPopupKind::Display);

  lv_obj_t* wifi_info = nullptr;
  next_settings_tile_cell(settings_col, settings_row, kSettingsMenuTileSpanW, tile_col, tile_row);
  lv_obj_t* wifi_tile =
      create_settings_menu_tile(tab, tile_col, tile_row, "wifi", tr().wifi_label,
                                &settings_tile_wifi_title, nullptr,
                                SettingsPopupKind::Wifi,
                                &wifi_info);

  next_settings_tile_cell(settings_col, settings_row, kSettingsMenuTileSpanW, tile_col, tile_row);
  create_settings_menu_tile(tab, tile_col, tile_row, "server", tr().admin_settings_mqtt,
                            &settings_tile_mqtt_title,
                            &settings_tile_mqtt_summary,
                            SettingsPopupKind::Mqtt);

  next_settings_tile_cell(settings_col, settings_row, kSettingsMenuTileSpanW, tile_col, tile_row);
  create_settings_menu_tile(tab, tile_col, tile_row, "translate", tr().admin_settings_language,
                            &settings_tile_locale_title,
                            &settings_tile_locale_summary,
                            SettingsPopupKind::Localization);

  next_settings_tile_cell(settings_col, settings_row, kSettingsMenuTileSpanW, tile_col, tile_row);
  create_settings_menu_tile(tab, tile_col, tile_row, "chip", "Firmware",
                            &settings_tile_firmware_title,
                            &settings_tile_firmware_summary,
                            SettingsPopupKind::Firmware);

  if (wifi_tile) {
    lv_obj_t* wifi_info_parent = wifi_info ? wifi_info : wifi_tile;
    wifi_status_label = lv_label_create(wifi_info_parent);
    lv_label_set_long_mode(wifi_status_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(wifi_status_label, LV_PCT(100));
    lv_obj_set_style_text_align(wifi_status_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(wifi_status_label, &ui_font_16, 0);
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFF6B6B), 0);

    wifi_ssid_label = lv_label_create(wifi_info_parent);
    lv_label_set_long_mode(wifi_ssid_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(wifi_ssid_label, LV_PCT(100));
    lv_obj_set_style_text_align(wifi_ssid_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(wifi_ssid_label, &ui_font_16, 0);
    lv_obj_set_style_text_color(wifi_ssid_label, lv_color_hex(0xA8A8A8), 0);

    wifi_ip_label = lv_label_create(wifi_info_parent);
    lv_label_set_long_mode(wifi_ip_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(wifi_ip_label, LV_PCT(100));
    lv_obj_set_style_text_align(wifi_ip_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(wifi_ip_label, &ui_font_16, 0);
    lv_obj_set_style_text_color(wifi_ip_label, lv_color_hex(0xA8A8A8), 0);

  }

  mains_wake_btn = nullptr;
  mains_wake_label = nullptr;
  mains_wake_sub_label = nullptr;
  battery_wake_btn = nullptr;
  battery_wake_label = nullptr;
  battery_wake_sub_label = nullptr;
  power_status_label = nullptr;
  power_level_label = nullptr;
  battery_icon_label = nullptr;
  battery_percent_label = nullptr;
  update_settings_tile_summaries();
  if (ap_mode_active) {
    String ap_ssid = WiFi.softAPSSID();
    settings_update_wifi_status_ap(ap_ssid.length() ? ap_ssid.c_str() : nullptr,
                                   webConfigApPassword());
  } else {
    String ssid = WiFi.SSID();
    String ip = WiFi.localIP().toString();
    settings_update_wifi_status(WiFi.status() == WL_CONNECTED,
                                ssid.length() ? ssid.c_str() : nullptr,
                                ip.length() ? ip.c_str() : nullptr);
  }
  settings_show_mqtt_warning(!configManager.hasMqttConfig());
}

void settings_update_wifi_status(bool connected, const char* ssid, const char* ip) {
  if (!wifi_status_label || !wifi_ssid_label || !wifi_ip_label) return;

  static char buf[128];

  if (connected) {
    format_status_line(buf, sizeof(buf), tr().wifi_connected);
    lv_label_set_text(wifi_status_label, buf);
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0x51CF66), 0);

    format_ssid_line(buf, sizeof(buf), ssid ? ssid : "---");
    lv_label_set_text(wifi_ssid_label, buf);

    format_ip_line(buf, sizeof(buf), ip ? ip : "---");
    lv_label_set_text(wifi_ip_label, buf);

  } else {
    format_status_line(buf, sizeof(buf), tr().wifi_offline);
    lv_label_set_text(wifi_status_label, buf);
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFF6B6B), 0);
    format_ssid_line(buf, sizeof(buf), "---");
    lv_label_set_text(wifi_ssid_label, buf);
    format_ip_line(buf, sizeof(buf), "---");
    lv_label_set_text(wifi_ip_label, buf);
  }
}

void settings_update_wifi_status_ap(const char* ssid, const char* password) {
  if (!wifi_status_label || !wifi_ssid_label || !wifi_ip_label) return;

  lv_obj_clear_flag(wifi_ssid_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(wifi_ip_label, LV_OBJ_FLAG_HIDDEN);

  static char status_buf[96];
  snprintf(status_buf, sizeof(status_buf), tr().wifi_status_ap_format, password ? password : "12345678");
  lv_label_set_text(wifi_status_label, status_buf);
  lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFFC04D), 0);

  static char buf[128];
  format_ssid_line(buf, sizeof(buf), ssid ? ssid : webConfigApSsid());
  lv_label_set_text(wifi_ssid_label, buf);

  String ip_str = WiFi.softAPIP().toString();
  format_ip_line(buf, sizeof(buf), ip_str.length() ? ip_str.c_str() : "192.168.4.1");
  lv_label_set_text(wifi_ip_label, buf);
}

void settings_show_mqtt_warning(bool show) {
  if (settings_tile_mqtt_summary) {
    update_settings_tile_summaries();
    lv_obj_set_style_text_color(settings_tile_mqtt_summary,
                                lv_color_hex(show ? 0xFFC04D : 0xA8A8A8),
                                0);
  }
  if (wifi_ssid_label) lv_obj_clear_flag(wifi_ssid_label, LV_OBJ_FLAG_HIDDEN);
  if (wifi_ip_label) lv_obj_clear_flag(wifi_ip_label, LV_OBJ_FLAG_HIDDEN);
}

void settings_update_ap_mode(bool running) {
  ap_mode_active = running;
  if (!ap_mode_btn_label && ap_mode_btn) {
    ap_mode_btn_label = lv_obj_get_child(ap_mode_btn, 0);
  }
  if (ap_mode_btn_label) {
    lv_label_set_text(ap_mode_btn_label, running ? tr().ap_disable : tr().ap_enable);
  }
  if (ap_mode_btn) {
    style_settings_button(ap_mode_btn, running ? 0xC62828 : 0xFF9800);
    if (!ap_mode_confirm_pending) {
      lv_obj_clear_flag(ap_mode_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (running) {
    hide_ap_confirm_row();
  }
}

void settings_refresh_language() {
  const auto& s = tr();

  if (settings_tile_display_title) lv_label_set_text(settings_tile_display_title, s.display_label);
  if (settings_tile_wifi_title) lv_label_set_text(settings_tile_wifi_title, s.wifi_label);
  if (settings_tile_mqtt_title) lv_label_set_text(settings_tile_mqtt_title, s.admin_settings_mqtt);
  if (settings_tile_locale_title) lv_label_set_text(settings_tile_locale_title, s.admin_settings_language);
  if (settings_tile_firmware_title) lv_label_set_text(settings_tile_firmware_title, "Firmware");
  if (settings_popup_title) lv_label_set_text(settings_popup_title, popup_title_for_kind(settings_popup_kind));
  if (settings_popup_save_label) lv_label_set_text(settings_popup_save_label, s.save);
  if (display_section_label) lv_label_set_text(display_section_label, s.display_label);
  if (brightness_title_label) lv_label_set_text(brightness_title_label, s.brightness_label);
  if (wifi_section_label) lv_label_set_text(wifi_section_label, s.wifi_label);
  if (sleep_section_label) lv_label_set_text(sleep_section_label, s.sleep_label);
  if (sleep_label) lv_label_set_text(sleep_label, s.sleep_label);
  if (ap_yes_label_obj) lv_label_set_text(ap_yes_label_obj, s.yes);
  if (ap_no_label_obj) lv_label_set_text(ap_no_label_obj, s.no);
  if (wifi_choose_btn_label) lv_label_set_text(wifi_choose_btn_label, s.wifi_choose_btn);

  update_wake_button(mains_wake_label, mains_wake_sub_label, kWakeModeTouch);
  update_wake_button(battery_wake_label, battery_wake_sub_label, kWakeModeTouch);
  settings_update_ap_mode(ap_mode_active);
  update_settings_tile_summaries();

  settings_show_mqtt_warning(!configManager.hasMqttConfig());

  if (ap_mode_active) {
    String ssid = WiFi.softAPSSID();
    settings_update_wifi_status_ap(ssid.length() ? ssid.c_str() : nullptr, webConfigApPassword());
  } else {
    String ssid = WiFi.SSID();
    String ip = WiFi.localIP().toString();
    settings_update_wifi_status(WiFi.status() == WL_CONNECTED,
                                ssid.length() ? ssid.c_str() : nullptr,
                                ip.length() ? ip.c_str() : nullptr);
  }
}

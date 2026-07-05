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
  Localization,
  Firmware,
};

static lv_obj_t *settings_tile_display_title = nullptr;
static lv_obj_t *settings_tile_display_summary = nullptr;
static lv_obj_t *settings_tile_wifi_title = nullptr;
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
// Icon-Label im X-Button oben rechts: wird in der WLAN-Eingabe-Ansicht zum
// Zurueck-Pfeil umfunktioniert (siehe on_settings_popup_close_clicked).
static lv_obj_t *settings_popup_close_icon = nullptr;
// Platzhalter, der in der Content-Spalte den Bereich der frei positionierten
// Tastatur reserviert - wird in der WLAN-Listen-Ansicht ausgeblendet, damit
// die Netzwerkliste die volle Hoehe nutzen kann.
static lv_obj_t *settings_popup_kb_spacer = nullptr;
static SettingsPopupKind settings_popup_kind = SettingsPopupKind::Display;
static bool settings_popup_restart_pending = false;

static lv_obj_t *wifi_ssid_ta = nullptr;
static lv_obj_t *wifi_pass_ta = nullptr;
static lv_obj_t *wifi_pass_eye_icon = nullptr;
static lv_obj_t *wifi_list_view = nullptr;
static lv_obj_t *wifi_entry_view = nullptr;
static lv_obj_t *wifi_conn_status_label = nullptr;
static lv_obj_t *wifi_scan_status_label = nullptr;
static lv_obj_t *wifi_list_container = nullptr;
static lv_obj_t *wifi_manual_row = nullptr;
static lv_obj_t *wifi_manual_gap = nullptr;
static lv_obj_t *wifi_ap_qr = nullptr;
static lv_timer_t *wifi_scan_timer = nullptr;
struct WifiScanEntry { char ssid[33]; int16_t rssi; bool open; };
static WifiScanEntry wifi_scan_results[24];
static size_t wifi_scan_result_count = 0;
static char wifi_selected_ssid[33] = {};
static bool wifi_selected_open = false;
static bool wifi_manual_mode = false;
static char wifi_known_fallback_ssid[33] = {};

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
static void wifi_stop_scan_timer();
static void wifi_show_list_view();
static void wifi_update_conn_status_label();

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
  lv_obj_set_style_radius(btn, 20, 0);
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

// In der WLAN-Eingabe-Ansicht ist die Tastatur fester Bestandteil des Screens
// (der Verbinden-Button sitzt direkt darueber) - dort darf sie weder ueber die
// Einklapp-Taste noch durch Tippen neben ein Feld verschwinden.
static bool wifi_keyboard_locked() {
  return settings_popup_kind == SettingsPopupKind::Wifi && wifi_entry_view &&
         !lv_obj_has_flag(wifi_entry_view, LV_OBJ_FLAG_HIDDEN);
}

// Tippt der Nutzer ausserhalb eines Feldes (auch Checkbox/Save/X-Button:
// per Default click-focusable), feuert LVGL hier DEFOCUSED, bevor der
// eigentliche Klick verarbeitet wird - blendet die Tastatur also zuverlaessig
// aus, ohne dass jeder Button das separat anstossen muesste.
static void on_popup_textarea_defocused(lv_event_t*) {
  if (wifi_keyboard_locked()) return;
  if (settings_popup_keyboard) lv_obj_add_flag(settings_popup_keyboard, LV_OBJ_FLAG_HIDDEN);
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

static void reset_popup_refs() {
  settings_popup_card = nullptr;
  settings_popup_title = nullptr;
  settings_popup_save_btn = nullptr;
  settings_popup_save_label = nullptr;
  settings_popup_content = nullptr;
  settings_popup_keyboard = nullptr;
  settings_popup_active_ta = nullptr;
  settings_popup_close_icon = nullptr;
  settings_popup_kb_spacer = nullptr;
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
  wifi_pass_eye_icon = nullptr;
  wifi_list_view = nullptr;
  wifi_entry_view = nullptr;
  wifi_conn_status_label = nullptr;
  wifi_scan_status_label = nullptr;
  wifi_list_container = nullptr;
  wifi_manual_row = nullptr;
  wifi_manual_gap = nullptr;
  wifi_ap_qr = nullptr;
  wifi_manual_mode = false;

  locale_language_dd = nullptr;
  locale_timezone_dd = nullptr;
  locale_time_format_dd = nullptr;
  locale_date_format_dd = nullptr;
}

static void close_settings_popup() {
  if (settings_popup_restart_pending) return;
  wifi_stop_scan_timer();
  WiFi.scanDelete();
  if (settings_popup_overlay) {
    lv_obj_del(settings_popup_overlay);
  }
  settings_popup_overlay = nullptr;
  reset_popup_refs();
}

static void on_settings_popup_close_clicked(lv_event_t*) {
  if (settings_popup_restart_pending) return;
  // In der WLAN-Eingabe-Ansicht ist der Kopf-Button ein Zurueck-Pfeil (Icon
  // wird in wifi_show_entry_view getauscht) und fuehrt zur Liste zurueck -
  // erst von dort schliesst er das Popup wirklich.
  if (settings_popup_kind == SettingsPopupKind::Wifi && wifi_entry_view &&
      !lv_obj_has_flag(wifi_entry_view, LV_OBJ_FLAG_HIDDEN)) {
    wifi_show_list_view();
    return;
  }
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

// WLAN-Liste/Eingabe-Statemachine, portiert aus dem vormals unverdrahteten
// wifi_setup_popup.cpp (Scan/Liste/Verbinden-Logik unveraendert uebernommen),
// aber an die bereits gefixte Tastatur (create_popup_keyboard) gehaengt statt
// eine zweite, eigene Tastatur-Instanz mit eigenen (ungefixten) Raendern zu
// bauen.
static void wifi_show_entry_view(bool manual, const char* ssid, bool open_network);
static void wifi_show_list_view();

static void wifi_stop_scan_timer() {
  if (wifi_scan_timer) {
    lv_timer_del(wifi_scan_timer);
    wifi_scan_timer = nullptr;
  }
}

// "Suche Netzwerke..." erscheint NUR, solange die Liste komplett leer ist
// (allererster Scan ohne bekanntes Netz) - sobald Zeilen stehen, laeuft jeder
// weitere Scan unsichtbar im Hintergrund, statt permanent einen Ladehinweis
// einzublenden. Der Container wird dabei nie vorab geleert (siehe
// wifi_on_scan_timer), damit die Liste nicht kurz leer aufblitzt.
static bool wifi_list_is_empty() {
  return !wifi_list_container || lv_obj_get_child_count(wifi_list_container) == 0;
}

static void wifi_show_scanning_state() {
  if (!wifi_scan_status_label) return;
  if (wifi_list_is_empty()) {
    lv_label_set_text(wifi_scan_status_label, tr().wifi_scan_searching);
    lv_obj_clear_flag(wifi_scan_status_label, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(wifi_scan_status_label, LV_OBJ_FLAG_HIDDEN);
  }
}

static void wifi_show_scan_finished_state() {
  if (!wifi_scan_status_label) return;
  if (wifi_list_is_empty()) {
    lv_label_set_text(wifi_scan_status_label, tr().wifi_scan_none);
    lv_obj_clear_flag(wifi_scan_status_label, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(wifi_scan_status_label, LV_OBJ_FLAG_HIDDEN);
  }
}

static void on_wifi_manual_clicked(lv_event_t*) {
  wifi_show_entry_view(true, nullptr, false);
}

static void on_wifi_network_row_clicked(lv_event_t* e) {
  const size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  if (index >= wifi_scan_result_count) return;
  wifi_show_entry_view(false, wifi_scan_results[index].ssid, wifi_scan_results[index].open);
}

static void on_wifi_known_row_clicked(lv_event_t*) {
  wifi_show_entry_view(false, wifi_known_fallback_ssid, false);
}

static lv_obj_t* wifi_create_row(lv_obj_t* parent, const char* name_text, bool show_check,
                                 bool show_lock) {
  lv_obj_t* row = lv_button_create(parent);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, 72);
  style_settings_button(row, 0x3A3A3A);
  lv_obj_set_style_border_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(row, 20, 0);
  lv_obj_set_style_pad_hor(row, 20, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 12, 0);

  if (show_check) {
    lv_obj_t* check = lv_label_create(row);
    lv_label_set_text(check, getMdiChar("check").c_str());
    if (FONT_MDI_ICONS) lv_obj_set_style_text_font(check, FONT_MDI_ICONS, 0);
    lv_obj_set_style_text_color(check, lv_color_hex(0x51CF66), 0);
  }

  lv_obj_t* name = lv_label_create(row);
  lv_label_set_text(name, name_text);
  lv_obj_set_style_text_font(name, &ui_font_24, 0);
  lv_obj_set_style_text_color(name, lv_color_white(), 0);
  lv_obj_set_flex_grow(name, 1);
  lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);

  if (show_lock) {
    lv_obj_t* lock = lv_label_create(row);
    lv_label_set_text(lock, getMdiChar("lock").c_str());
    if (FONT_MDI_ICONS) lv_obj_set_style_text_font(lock, FONT_MDI_ICONS, 0);
    lv_obj_set_style_text_color(lock, lv_color_hex(0x888888), 0);
  }
  return row;
}

// Bekanntes/verbundenes Netz wird IMMER angezeigt, auch bevor/ohne dass ein
// Scan es findet (versteckte SSIDs tauchen bei WiFi.scanNetworks() per
// Default gar nicht erst auf) - sonst verschwindet ein gerade manuell
// verbundenes Netz aus der Liste, sobald der naechste Scan durchlaeuft.
static void wifi_populate_list() {
  if (!wifi_list_container) return;
  lv_obj_clean(wifi_list_container);

  const bool sta_connected = !ap_mode_active && WiFi.status() == WL_CONNECTED;
  const String current_ssid = sta_connected ? WiFi.SSID()
                                            : String(configManager.getConfig().wifi_ssid);

  bool current_in_results = false;
  for (size_t i = 0; i < wifi_scan_result_count; ++i) {
    const bool is_current = current_ssid.length() && current_ssid == wifi_scan_results[i].ssid;
    if (is_current) current_in_results = true;
    lv_obj_t* row = wifi_create_row(wifi_list_container, wifi_scan_results[i].ssid,
                                    is_current && sta_connected, !wifi_scan_results[i].open);
    lv_obj_add_event_cb(row, on_wifi_network_row_clicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
  }

  if (current_ssid.length() && !current_in_results) {
    strncpy(wifi_known_fallback_ssid, current_ssid.c_str(), sizeof(wifi_known_fallback_ssid) - 1);
    wifi_known_fallback_ssid[sizeof(wifi_known_fallback_ssid) - 1] = '\0';
    lv_obj_t* row = wifi_create_row(wifi_list_container, wifi_known_fallback_ssid,
                                    sta_connected, true);
    lv_obj_add_event_cb(row, on_wifi_known_row_clicked, LV_EVENT_CLICKED, nullptr);
  }
}

static void wifi_on_scan_timer(lv_timer_t*) {
  int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  wifi_stop_scan_timer();

  wifi_scan_result_count = 0;
  for (int16_t i = 0; i < n && wifi_scan_result_count < 24; ++i) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    const int16_t rssi = static_cast<int16_t>(WiFi.RSSI(i));
    const bool open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);

    bool duplicate = false;
    for (size_t k = 0; k < wifi_scan_result_count; ++k) {
      if (strcmp(wifi_scan_results[k].ssid, ssid.c_str()) == 0) {
        duplicate = true;
        if (rssi > wifi_scan_results[k].rssi) {
          wifi_scan_results[k].rssi = rssi;
          wifi_scan_results[k].open = open;
        }
        break;
      }
    }
    if (duplicate) continue;

    WifiScanEntry& entry = wifi_scan_results[wifi_scan_result_count++];
    strncpy(entry.ssid, ssid.c_str(), sizeof(entry.ssid) - 1);
    entry.ssid[sizeof(entry.ssid) - 1] = '\0';
    entry.rssi = rssi;
    entry.open = open;
  }
  WiFi.scanDelete();

  // Nach Signalstaerke sortieren
  for (size_t i = 1; i < wifi_scan_result_count; ++i) {
    WifiScanEntry key = wifi_scan_results[i];
    size_t j = i;
    while (j > 0 && wifi_scan_results[j - 1].rssi < key.rssi) {
      wifi_scan_results[j] = wifi_scan_results[j - 1];
      --j;
    }
    wifi_scan_results[j] = key;
  }

  wifi_populate_list();
  wifi_show_scan_finished_state();
}

// Alte Ergebnisse bleiben absichtlich stehen (siehe wifi_show_scanning_state) -
// nur der Zaehler wird NICHT vorab genullt, damit die Liste bis zum
// naechsten fertigen Scan nicht kurz leer aufblitzt.
static void wifi_start_scan() {
  wifi_stop_scan_timer();
  // Erst die Liste fuellen (bekanntes Netz erscheint sofort), dann
  // entscheiden, ob der "Suche..."-Hinweis ueberhaupt noetig ist.
  wifi_populate_list();
  // Kein Scan im AP-Modus: WiFi.scanNetworks() schaltet STA dazu und ein
  // noch laufender Scan laesst spaeter WiFi.begin()/den Portal-Betrieb ins
  // Leere laufen (deshalb verband sich das Geraet nach "AP beenden" nicht
  // mehr automatisch).
  if (ap_mode_active) {
    wifi_show_scan_finished_state();
    return;
  }
  wifi_show_scanning_state();
  WiFi.scanDelete();
  const int16_t r = WiFi.scanNetworks(/*async=*/true);
  if (r == WIFI_SCAN_FAILED) {
    wifi_show_scan_finished_state();
    return;
  }
  wifi_scan_timer = lv_timer_create(wifi_on_scan_timer, 500, nullptr);
}

static void wifi_update_conn_status_label() {
  if (!wifi_conn_status_label) return;
  static char buf[96];

  // Im AP-Modus sind Scan und Netzwerkliste nutzlos (Scans sind waehrend des
  // Portal-Betriebs abgeschaltet, siehe wifi_start_scan) - die Liste weicht
  // der grossen Infobox mit Zugangsdaten + QR-Code.
  const bool ap = ap_mode_active;
  if (wifi_list_container) {
    if (ap) lv_obj_add_flag(wifi_list_container, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(wifi_list_container, LV_OBJ_FLAG_HIDDEN);
  }
  if (wifi_manual_gap) {
    if (ap) lv_obj_add_flag(wifi_manual_gap, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(wifi_manual_gap, LV_OBJ_FLAG_HIDDEN);
  }
  if (wifi_manual_row) {
    if (ap) lv_obj_add_flag(wifi_manual_row, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(wifi_manual_row, LV_OBJ_FLAG_HIDDEN);
  }
  if (ap && wifi_scan_status_label) lv_obj_add_flag(wifi_scan_status_label, LV_OBJ_FLAG_HIDDEN);

  if (ap) {
    static char ap_status[64];
    snprintf(ap_status, sizeof(ap_status), tr().wifi_status_ap_format, webConfigApPassword());
    // Direkt nach dem Einschalten liefert softAPIP() noch "0.0.0.0" -
    // dann die AP-Standard-IP anzeigen statt Muell.
    String ap_ip = WiFi.softAPIP().toString();
    const bool ip_valid = ap_ip.length() && ap_ip != "0.0.0.0";
    snprintf(buf, sizeof(buf), "%s\n%s: %s - %s: %s", ap_status,
             tr().ssid_label, webConfigApSsid(),
             tr().ip_label, ip_valid ? ap_ip.c_str() : "192.168.4.1");
    lv_label_set_text(wifi_conn_status_label, buf);
    lv_obj_set_style_text_color(wifi_conn_status_label, lv_color_hex(0xFFC04D), 0);
#if LV_USE_QRCODE
    if (wifi_ap_qr) {
      // Handy-Kameras verbinden sich damit direkt mit dem Hotspot
      static char qr_buf[128];
      snprintf(qr_buf, sizeof(qr_buf), "WIFI:T:WPA;S:%s;P:%s;;",
               webConfigApSsid(), webConfigApPassword());
      lv_qrcode_update(wifi_ap_qr, qr_buf, strlen(qr_buf));
      lv_obj_clear_flag(wifi_ap_qr, LV_OBJ_FLAG_HIDDEN);
    }
#endif
    return;
  }

#if LV_USE_QRCODE
  if (wifi_ap_qr) lv_obj_add_flag(wifi_ap_qr, LV_OBJ_FLAG_HIDDEN);
#endif
  if (WiFi.status() == WL_CONNECTED) {
    String ssid = WiFi.SSID();
    String ip = WiFi.localIP().toString();
    snprintf(buf, sizeof(buf), "%s: %s (%s)", tr().wifi_connected,
             ssid.length() ? ssid.c_str() : "---", ip.c_str());
    lv_label_set_text(wifi_conn_status_label, buf);
    lv_obj_set_style_text_color(wifi_conn_status_label, lv_color_hex(0x51CF66), 0);
  } else {
    lv_label_set_text(wifi_conn_status_label, tr().wifi_offline);
    lv_obj_set_style_text_color(wifi_conn_status_label, lv_color_hex(0xFF6B6B), 0);
  }
}

// Jeder Aufruf (Erstoeffnen wie auch "Zurueck" aus der Eingabe-Ansicht)
// stoesst einen frischen Scan an - ersetzt den frueheren separaten
// Rescan-Icon-Button, den der Nutzer als unnoetigen Ballast empfand.
static void wifi_show_list_view() {
  if (wifi_list_view) lv_obj_clear_flag(wifi_list_view, LV_OBJ_FLAG_HIDDEN);
  if (wifi_entry_view) lv_obj_add_flag(wifi_entry_view, LV_OBJ_FLAG_HIDDEN);
  if (settings_popup_keyboard) lv_obj_add_flag(settings_popup_keyboard, LV_OBJ_FLAG_HIDDEN);
  // Ohne Tastatur braucht die Liste den reservierten Bereich nicht -
  // ausgeblendet nutzt sie die volle Popup-Hoehe.
  if (settings_popup_kb_spacer) lv_obj_add_flag(settings_popup_kb_spacer, LV_OBJ_FLAG_HIDDEN);
  if (settings_popup_close_icon) {
    lv_label_set_text(settings_popup_close_icon, getMdiChar("window-close").c_str());
  }
  settings_popup_active_ta = nullptr;
  wifi_update_conn_status_label();
  wifi_start_scan();
}

static void wifi_show_entry_view(bool manual, const char* ssid, bool open_network) {
  wifi_manual_mode = manual;
  wifi_selected_open = open_network;
  if (ssid) {
    strncpy(wifi_selected_ssid, ssid, sizeof(wifi_selected_ssid) - 1);
    wifi_selected_ssid[sizeof(wifi_selected_ssid) - 1] = '\0';
  } else {
    wifi_selected_ssid[0] = '\0';
  }
  const bool hide_password = !manual && open_network;

  // Einheitliches Layout: das SSID-Feld ist IMMER sichtbar. Aus der Liste
  // kommend ist es vorbefuellt und nicht antippbar (nur bei "Manuell"
  // editierbar) - dezente Farben signalisieren den Nur-Lese-Zustand.
  if (wifi_ssid_ta) {
    lv_textarea_set_text(wifi_ssid_ta, manual ? "" : wifi_selected_ssid);
    if (manual) {
      lv_obj_add_flag(wifi_ssid_ta, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_text_color(wifi_ssid_ta, lv_color_white(), 0);
      lv_obj_set_style_bg_color(wifi_ssid_ta, lv_color_hex(0x1E1E1E), 0);
    } else {
      lv_obj_clear_flag(wifi_ssid_ta, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_text_color(wifi_ssid_ta, lv_color_hex(0xB8B8B8), 0);
      lv_obj_set_style_bg_color(wifi_ssid_ta, lv_color_hex(0x2A2A2A), 0);
    }
  }

  if (wifi_pass_ta) {
    // Bereits gespeichertes Passwort vorausfuellen, wenn das angetippte Netz
    // das aktuell konfigurierte ist - sonst wirkt das Feld beim erneuten
    // Oeffnen des eigenen Netzes so, als waere gar kein Passwort hinterlegt.
    const DeviceConfig& known_cfg = configManager.getConfig();
    const bool is_known_ssid = !manual && wifi_selected_ssid[0] &&
                              strcmp(wifi_selected_ssid, known_cfg.wifi_ssid) == 0;
    lv_textarea_set_text(wifi_pass_ta, is_known_ssid ? known_cfg.wifi_pass : "");
    lv_textarea_set_password_mode(wifi_pass_ta, true);
    if (wifi_pass_eye_icon) lv_label_set_text(wifi_pass_eye_icon, getMdiChar("eye").c_str());
    lv_obj_t* pass_box = lv_obj_get_parent(wifi_pass_ta);
    if (pass_box) {
      if (hide_password) lv_obj_add_flag(pass_box, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_clear_flag(pass_box, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (wifi_list_view) lv_obj_add_flag(wifi_list_view, LV_OBJ_FLAG_HIDDEN);
  if (wifi_entry_view) lv_obj_clear_flag(wifi_entry_view, LV_OBJ_FLAG_HIDDEN);

  // Kopf-Button wird zum Zurueck-Pfeil (statt eines eigenen Zurueck-Links im
  // Body) - Klick-Logik siehe on_settings_popup_close_clicked.
  if (settings_popup_close_icon) {
    lv_label_set_text(settings_popup_close_icon, getMdiChar("arrow-left").c_str());
  }

  // Anders als bei jedem anderen Popup-Feld wird die Tastatur hier sofort
  // gezeigt statt erst beim Antippen eines Feldes - dieser Screen dient fast
  // ausschliesslich der Texteingabe. Der Layout-Platzhalter wandert synchron
  // mit, damit der Verbinden-Button nicht unter der Tastatur landet.
  if (settings_popup_keyboard) {
    if (hide_password) {
      lv_obj_add_flag(settings_popup_keyboard, LV_OBJ_FLAG_HIDDEN);
      if (settings_popup_kb_spacer) lv_obj_add_flag(settings_popup_kb_spacer, LV_OBJ_FLAG_HIDDEN);
      settings_popup_active_ta = nullptr;
    } else {
      lv_keyboard_set_mode(settings_popup_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
      lv_obj_t* first = manual ? wifi_ssid_ta : wifi_pass_ta;
      lv_obj_t* second = manual ? wifi_pass_ta : wifi_ssid_ta;
      ui_keyboard_set_target(settings_popup_keyboard, first, second);
      settings_popup_active_ta = first;
      if (settings_popup_kb_spacer) lv_obj_clear_flag(settings_popup_kb_spacer, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(settings_popup_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void on_wifi_pass_eye_clicked(lv_event_t* e) {
  lv_obj_t* icon = static_cast<lv_obj_t*>(lv_event_get_target(e));
  lv_obj_t* ta = icon ? lv_obj_get_parent(icon) : nullptr;
  if (!ta) return;
  const bool currently_masked = lv_textarea_get_password_mode(ta);
  lv_textarea_set_password_mode(ta, !currently_masked);
  lv_label_set_text(icon, getMdiChar(currently_masked ? "eye-off" : "eye").c_str());
}

static void wifi_do_connect() {
  if (settings_popup_restart_pending) return;
  const char* ssid = wifi_manual_mode && wifi_ssid_ta ? lv_textarea_get_text(wifi_ssid_ta)
                                                      : wifi_selected_ssid;
  const char* pass = wifi_pass_ta ? lv_textarea_get_text(wifi_pass_ta) : "";
  if (!ssid || !ssid[0]) return;
  // Gesichertes Netz ohne Passwort: nicht speichern (wuerde nach dem
  // Neustart nur in einer toten Verbindung enden)
  if (!wifi_manual_mode && !wifi_selected_open && (!pass || !pass[0])) return;

  // Unveraenderte Zugangsdaten: kein unnoetiges Speichern+Neustarten.
  // Aus dem AP-Modus heraus reicht "AP aus" - der Hauptloop verbindet dann
  // automatisch neu (apply_hotspot_mode).
  const DeviceConfig& cur = configManager.getConfig();
  if (strcmp(cur.wifi_ssid, ssid) == 0 && strcmp(cur.wifi_pass, pass ? pass : "") == 0) {
    if (ap_mode_active) {
      wifi_stop_scan_timer();
      WiFi.scanDelete();
      if (g_hotspot_callback) g_hotspot_callback(false);
      close_settings_popup();
      return;
    }
    if (WiFi.status() == WL_CONNECTED) {
      close_settings_popup();
      return;
    }
    // offline mit denselben Daten: unten regulaer speichern + neu starten
  }

  DeviceConfig cfg = configManager.getConfig();
  strncpy(cfg.wifi_ssid, ssid, CONFIG_WIFI_SSID_MAX - 1);
  cfg.wifi_ssid[CONFIG_WIFI_SSID_MAX - 1] = '\0';
  strncpy(cfg.wifi_pass, pass ? pass : "", CONFIG_WIFI_PASS_MAX - 1);
  cfg.wifi_pass[CONFIG_WIFI_PASS_MAX - 1] = '\0';
  // Netzwechsel setzt auf DHCP zurueck, damit eine alte statische IP das
  // Geraet im neuen Netz nicht aussperrt (statische IP gibt's nur noch im
  // Web-Admin).
  cfg.wifi_static_ip[0] = '\0';
  cfg.wifi_gateway[0] = '\0';
  cfg.wifi_subnet[0] = '\0';
  cfg.wifi_dns[0] = '\0';

  if (!configManager.save(cfg)) {
    if (settings_popup_title) lv_label_set_text(settings_popup_title, tr().wifi_save_failed);
    return;
  }
  mark_popup_restarting();
}

// Kopf bleibt immer gleich (Icon+"WLAN"+Speichern+X, siehe open_settings_popup) -
// "Speichern" verbindet nur, wenn der Eingabe-Screen offen ist, auf dem
// Listen-Screen tut es nichts.
static void save_wifi_popup() {
  if (wifi_entry_view && !lv_obj_has_flag(wifi_entry_view, LV_OBJ_FLAG_HIDDEN)) {
    wifi_do_connect();
  }
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
    // ungesicherte Aenderungen verworfen hat). In der WLAN-Eingabe bleibt die
    // Tastatur grundsaetzlich stehen.
    if (wifi_keyboard_locked()) return;
    hide_popup_keyboard();
  }
}

static void on_popup_ap_mode_clicked(lv_event_t*) {
  if (ap_mode_click_block_until != 0 &&
      (int32_t)(millis() - ap_mode_click_block_until) < 0) {
    return;
  }
  // Laufenden Scan sofort beenden - der eigentliche Moduswechsel passiert
  // asynchron im Hauptloop und darf nicht mit einem Scan kollidieren.
  wifi_stop_scan_timer();
  WiFi.scanDelete();
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
  // Erst NACH der Messung merken/ausblendbar machen - die Tastaturgroesse
  // haengt von der sichtbaren 42%-Hoehe ab.
  settings_popup_kb_spacer = spacer;

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

// Eingabezeile "Label | Textfeld" in EINER Reihe (statt Label ueber dem
// Feld) - kompakter, damit auf dem 4-Zoll-B4 nichts abgeschnitten wird.
// Feldhoehe bewusst LV_SIZE_CONTENT + pad_ver statt fester Pixel: so sitzen
// Text und Cursor konstruktionsbedingt exakt mittig.
static lv_obj_t* wifi_create_entry_row(lv_obj_t* parent, const char* label_text,
                                       lv_obj_t** ta_out, uint16_t max_len,
                                       bool password) {
  lv_obj_t* row = lv_obj_create(parent);
  style_plain_container(row);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 14, 0);

  lv_obj_t* label = lv_label_create(row);
  lv_label_set_text(label, label_text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(label, 160);
  lv_obj_set_style_text_font(label, &ui_font_24, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xC8C8C8), 0);

  lv_obj_t* ta = lv_textarea_create(row);
  style_popup_textarea(ta);
  // flex_grow im ROW-Elternobjekt = restliche Breite (Hauptachse)
  lv_obj_set_flex_grow(ta, 1);
  lv_textarea_set_max_length(ta, max_len);
  lv_textarea_set_password_mode(ta, password);
  lv_textarea_set_placeholder_text(ta, "");
  lv_textarea_set_text(ta, "");
  // PRESSED zusaetzlich zu FOCUSED: FOCUSED haengt an LVGLs interner
  // "last_pressed"-Objektverfolgung und feuert in manchen Abfolgen nicht
  // zuverlaessig - PRESSED reagiert dagegen immer sofort auf den Touch.
  lv_obj_add_event_cb(ta, on_popup_textarea_focused, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(ta, on_popup_textarea_focused, LV_EVENT_FOCUSED, nullptr);
  lv_obj_add_event_cb(ta, on_popup_textarea_defocused, LV_EVENT_DEFOCUSED, nullptr);

  lv_obj_set_height(ta, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_ver(ta, 20, 0);
  lv_obj_set_style_pad_left(ta, 20, 0);
  lv_obj_set_style_radius(ta, 18, 0);
  lv_obj_set_style_text_font(ta, &ui_font_28, 0);
  // lv_textarea ist intern scrollbar - ohne das hier zeigt LVGL einen
  // Scrollbalken an der Feldkante.
  lv_obj_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_OFF);

  *ta_out = ta;
  return row;
}

static void build_wifi_popup(lv_obj_t* parent) {
  // ===== Listen-Ansicht: Liste kompakt oben (Manuell direkt darunter),
  // Infobox + AP-Button als Fussbereich unten =====
  wifi_list_view = lv_obj_create(parent);
  style_plain_container(wifi_list_view);
  lv_obj_set_width(wifi_list_view, LV_PCT(100));
  lv_obj_set_flex_grow(wifi_list_view, 1);
  lv_obj_clear_flag(wifi_list_view, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(wifi_list_view, LV_FLEX_FLOW_COLUMN);
  // Querachse CENTER: zentriert den (nicht mehr vollbreiten) AP-Button,
  // vollbreite Kinder sind davon unbeeindruckt.
  lv_obj_set_flex_align(wifi_list_view, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(wifi_list_view, 10, 0);

  wifi_scan_status_label = lv_label_create(wifi_list_view);
  lv_label_set_text(wifi_scan_status_label, tr().wifi_scan_searching);
  lv_obj_set_style_text_font(wifi_scan_status_label, &ui_font_20, 0);
  lv_obj_set_style_text_color(wifi_scan_status_label, lv_color_hex(0xA0A0A0), 0);

  // Liste ist nur so hoch wie ihr Inhalt (max. knapp die halbe Ansicht,
  // darueber hinaus scrollt sie) - so rueckt "Manuell" direkt unter die
  // Ergebnisse statt am unteren Rand zu kleben.
  wifi_list_container = lv_obj_create(wifi_list_view);
  style_plain_container(wifi_list_container);
  lv_obj_set_width(wifi_list_container, LV_PCT(100));
  lv_obj_set_height(wifi_list_container, LV_SIZE_CONTENT);
  lv_obj_set_style_max_height(wifi_list_container, LV_PCT(44), 0);
  lv_obj_add_flag(wifi_list_container, LV_OBJ_FLAG_SCROLLABLE);
  // Scrollen per Touch bleibt, nur der Balken verschwindet.
  lv_obj_set_scrollbar_mode(wifi_list_container, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(wifi_list_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(wifi_list_container, 8, 0);

  // Ausserhalb des scrollbaren Containers - steht immer fest unter der Liste
  // (statt bei jedem Rescan mit den Ergebnissen neu erzeugt zu werden) und ist
  // durch den Gap optisch abgesetzt.
  wifi_manual_gap = lv_obj_create(wifi_list_view);
  style_plain_container(wifi_manual_gap);
  lv_obj_clear_flag(wifi_manual_gap, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_width(wifi_manual_gap, LV_PCT(100));
  lv_obj_set_height(wifi_manual_gap, 6);

  wifi_manual_row = wifi_create_row(wifi_list_view, tr().wifi_manual_entry, false, false);
  lv_obj_add_event_cb(wifi_manual_row, on_wifi_manual_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* manual_chevron = lv_label_create(wifi_manual_row);
  lv_label_set_text(manual_chevron, getMdiChar("chevron-right").c_str());
  if (FONT_MDI_ICONS) lv_obj_set_style_text_font(manual_chevron, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(manual_chevron, lv_color_hex(0x888888), 0);

  // Spacer drueckt den Fussbereich (Infobox + AP-Button) nach unten.
  create_flex_spacer(wifi_list_view);

  // Infobox: abgesetzte Karte mit Verbindungsstatus; im AP-Modus zusaetzlich
  // QR-Code zum direkten Verbinden mit dem Hotspot.
  lv_obj_t* info_box = lv_obj_create(wifi_list_view);
  style_plain_container(info_box);
  lv_obj_set_width(info_box, LV_PCT(100));
  lv_obj_set_height(info_box, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(info_box, lv_color_hex(0x333333), 0);
  lv_obj_set_style_bg_opa(info_box, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(info_box, 20, 0);
  lv_obj_set_style_pad_all(info_box, 18, 0);
  lv_obj_set_style_pad_row(info_box, 16, 0);
  lv_obj_set_flex_flow(info_box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(info_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  wifi_conn_status_label = lv_label_create(info_box);
  lv_obj_set_width(wifi_conn_status_label, LV_PCT(100));
  lv_label_set_long_mode(wifi_conn_status_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(wifi_conn_status_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(wifi_conn_status_label, &ui_font_24, 0);

#if LV_USE_QRCODE
  wifi_ap_qr = lv_qrcode_create(info_box);
  lv_qrcode_set_size(wifi_ap_qr, 150);
  lv_qrcode_set_dark_color(wifi_ap_qr, lv_color_black());
  lv_qrcode_set_light_color(wifi_ap_qr, lv_color_white());
  lv_qrcode_set_quiet_zone(wifi_ap_qr, true);
  lv_obj_add_flag(wifi_ap_qr, LV_OBJ_FLAG_HIDDEN);
#endif

  // Mittig, hoeher, aber nicht mehr vollbreit
  ap_mode_btn = create_popup_button(wifi_list_view, ap_mode_active ? tr().ap_disable : tr().ap_enable,
                                    ap_mode_active ? 0xC62828 : 0xFF9800,
                                    on_popup_ap_mode_clicked);
  lv_obj_set_size(ap_mode_btn, 360, 76);
  ap_mode_btn_label = lv_obj_get_child(ap_mode_btn, 0);
  if (ap_mode_btn_label) lv_obj_set_style_text_font(ap_mode_btn_label, &ui_font_24, 0);

  // ===== Eingabe-Ansicht: SSID-/Passwort-Zeilen, Verbinden-Button unten =====
  wifi_entry_view = lv_obj_create(parent);
  style_plain_container(wifi_entry_view);
  lv_obj_set_width(wifi_entry_view, LV_PCT(100));
  lv_obj_set_flex_grow(wifi_entry_view, 1);
  lv_obj_clear_flag(wifi_entry_view, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(wifi_entry_view, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(wifi_entry_view, 14, 0);
  lv_obj_add_flag(wifi_entry_view, LV_OBJ_FLAG_HIDDEN);

  wifi_create_entry_row(wifi_entry_view, tr().ssid_label, &wifi_ssid_ta,
                        CONFIG_WIFI_SSID_MAX - 1, false);
  wifi_create_entry_row(wifi_entry_view, tr().wifi_password_label, &wifi_pass_ta,
                        CONFIG_WIFI_PASS_MAX - 1, true);
  lv_obj_set_style_pad_right(wifi_pass_ta, 64, 0);
  wifi_pass_eye_icon = lv_label_create(wifi_pass_ta);
  lv_label_set_text(wifi_pass_eye_icon, getMdiChar("eye").c_str());
  if (FONT_MDI_ICONS) lv_obj_set_style_text_font(wifi_pass_eye_icon, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(wifi_pass_eye_icon, lv_color_hex(0x888888), 0);
  // FLOATING: sonst zaehlt das Icon in die LV_SIZE_CONTENT-Hoehe des Feldes
  // hinein. lv_obj_align rechnet das Eltern-Padding mit ein (siehe
  // create_popup_keyboard) - der positive Offset kompensiert pad_right(64),
  // damit das Icon wirklich an der Feldkante sitzt (18px Abstand) statt links
  // neben dem Textbereich zu haengen.
  lv_obj_add_flag(wifi_pass_eye_icon, LV_OBJ_FLAG_FLOATING);
  lv_obj_align(wifi_pass_eye_icon, LV_ALIGN_RIGHT_MID, 64 - 18, 0);
  lv_obj_add_flag(wifi_pass_eye_icon, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(wifi_pass_eye_icon, on_wifi_pass_eye_clicked, LV_EVENT_CLICKED, nullptr);

  // Spacer drueckt den Verbinden-Button an den unteren Rand der Ansicht
  // (direkt ueber die Tastatur) - "alle Buttons nach unten".
  create_flex_spacer(wifi_entry_view);

  // Eigener, klar beschrifteter Verbinden-Button statt des generischen
  // Speichern oben rechts. Nutzt denselben Save-Dispatch wie die Tastatur-
  // Enter-Taste (on_settings_popup_save_clicked -> save_wifi_popup).
  lv_obj_t* wifi_connect_btn = create_popup_button(wifi_entry_view, tr().wifi_connect_btn, 0x2E7D32,
                                                   on_settings_popup_save_clicked);
  lv_obj_set_width(wifi_connect_btn, LV_PCT(100));
  lv_obj_set_height(wifi_connect_btn, 72);
  lv_obj_t* connect_label = lv_obj_get_child(wifi_connect_btn, 0);
  if (connect_label) lv_obj_set_style_text_font(connect_label, &ui_font_24, 0);

  // Tastatur zuerst (legt den Platzhalter an), dann die Listen-Ansicht
  // aktivieren - die blendet den Platzhalter direkt wieder aus.
  create_popup_keyboard(parent);
  wifi_show_list_view();
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

  // WiFi hat unten in der Eingabe-Ansicht einen eigenen, klar beschrifteten
  // "Verbinden"-Button (siehe build_wifi_popup) - der generische Speichern
  // oben rechts war dort nur verwirrend ("was macht der Button ueberhaupt"),
  // weil er in der Listen-Ansicht nichts tut. Fuer Localization bleibt er wie
  // gehabt oben.
  const bool show_top_save = popup_kind_has_save(kind) && kind != SettingsPopupKind::Wifi;

  settings_popup_title = lv_label_create(header);
  lv_label_set_text(settings_popup_title, popup_title_for_kind(kind));
  lv_label_set_long_mode(settings_popup_title, LV_LABEL_LONG_DOT);
  lv_obj_set_width(settings_popup_title, show_top_save ? LV_PCT(46) : LV_PCT(62));
  lv_obj_set_style_text_font(settings_popup_title, &ui_font_24, 0);
  lv_obj_set_style_text_color(settings_popup_title, lv_color_white(), 0);
  lv_obj_align(settings_popup_title, LV_ALIGN_LEFT_MID, 78, 0);

  settings_popup_save_btn = create_popup_button(settings_popup_card, tr().save, 0x2E7D32,
                                                on_settings_popup_save_clicked);
  lv_obj_add_flag(settings_popup_save_btn, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_width(settings_popup_save_btn, 150);
  lv_obj_align(settings_popup_save_btn, LV_ALIGN_TOP_RIGHT, -108, 10);
  settings_popup_save_label = lv_obj_get_child(settings_popup_save_btn, 0);
  if (!show_top_save) {
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
  lv_obj_set_style_radius(close_btn, 20, 0);
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
  settings_popup_close_icon = close_label;

  settings_popup_content = lv_obj_create(settings_popup_card);
  lv_obj_set_width(settings_popup_content, LV_PCT(100));
  lv_obj_set_flex_grow(settings_popup_content, 1);
  style_plain_container(settings_popup_content);
  lv_obj_set_style_pad_all(settings_popup_content, 0, 0);
  lv_obj_set_style_pad_row(settings_popup_content, 8, 0);
  // Luft zum Header: der X/Zurueck-Button (96px + ext_click_area) ragt unter
  // die Header-Kante - ohne Abstand ueberlappt sein Press-Highlight den
  // Inhalt oben rechts.
  lv_obj_set_style_pad_top(settings_popup_content, 14, 0);
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
  lv_obj_set_style_pad_row(info, 4, 0);
  if (info_container_out) *info_container_out = info;

  if (summary_label_out) {
    lv_obj_t* summary = lv_label_create(info);
    lv_label_set_text(summary, "");
    lv_label_set_long_mode(summary, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(summary, LV_PCT(100));
    lv_obj_set_style_text_align(summary, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(summary, &ui_font_20, 0);
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
    // Eine Info pro Zeile mit Beschriftung statt kryptischem "51% / Nie"
    snprintf(buf, sizeof(buf), "%s: %d%%\n%s: %s", tr().brightness_label,
             brightness_pct_from_raw(BoardHAL::getBrightness()),
             tr().sleep_label, sleep_buf);
    lv_label_set_text(settings_tile_display_summary, buf);
  }

  if (settings_tile_locale_summary) {
    const char* language = (i18n::normalize_language_code(cfg.language)[0] == 'd')
                               ? tr().language_option_german
                               : tr().language_option_english;
    // Nur der Stadtname - das volle "UTC+1 / UTC+2 - Berlin" wurde auf der
    // Kachel abgeschnitten ("Deutsch / UTC+1 / U").
    const char* tz_label = timezone_label_for_code(cfg.timezone);
    const char* city = strstr(tz_label, " - ");
    snprintf(buf, sizeof(buf), "%s\n%s", language, city ? city + 3 : tz_label);
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
    lv_obj_set_style_text_font(wifi_status_label, &ui_font_20, 0);
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFF6B6B), 0);

    wifi_ssid_label = lv_label_create(wifi_info_parent);
    lv_label_set_long_mode(wifi_ssid_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(wifi_ssid_label, LV_PCT(100));
    lv_obj_set_style_text_align(wifi_ssid_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(wifi_ssid_label, &ui_font_20, 0);
    lv_obj_set_style_text_color(wifi_ssid_label, lv_color_hex(0xA8A8A8), 0);

    wifi_ip_label = lv_label_create(wifi_info_parent);
    lv_label_set_long_mode(wifi_ip_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(wifi_ip_label, LV_PCT(100));
    lv_obj_set_style_text_align(wifi_ip_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(wifi_ip_label, &ui_font_20, 0);
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
}

// Kachel-Zeilen ohne "Status:/SSID:/IP:"-Praefixe - die Werte erklaeren sich
// selbst ("Verbunden" gruen, darunter Netzname und IP). Offline werden die
// leeren Zeilen ganz ausgeblendet statt "---" zu zeigen.
void settings_update_wifi_status(bool connected, const char* ssid, const char* ip) {
  if (!wifi_status_label || !wifi_ssid_label || !wifi_ip_label) return;

  if (connected) {
    lv_label_set_text(wifi_status_label, tr().wifi_connected);
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0x51CF66), 0);
    lv_label_set_text(wifi_ssid_label, ssid ? ssid : "---");
    lv_label_set_text(wifi_ip_label, ip ? ip : "---");
    lv_obj_clear_flag(wifi_ssid_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(wifi_ip_label, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_label_set_text(wifi_status_label, tr().wifi_offline);
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFF6B6B), 0);
    lv_obj_add_flag(wifi_ssid_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(wifi_ip_label, LV_OBJ_FLAG_HIDDEN);
  }
  // Infobox im offenen WLAN-Popup lebendig halten (z.B. Auto-Reconnect
  // nach "AP beenden" wird sofort sichtbar)
  wifi_update_conn_status_label();
}

void settings_update_wifi_status_ap(const char* ssid, const char* password) {
  if (!wifi_status_label || !wifi_ssid_label || !wifi_ip_label) return;

  lv_obj_clear_flag(wifi_ssid_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(wifi_ip_label, LV_OBJ_FLAG_HIDDEN);

  static char status_buf[96];
  snprintf(status_buf, sizeof(status_buf), tr().wifi_status_ap_format, password ? password : "12345678");
  lv_label_set_text(wifi_status_label, status_buf);
  lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFFC04D), 0);

  lv_label_set_text(wifi_ssid_label, ssid ? ssid : webConfigApSsid());

  String ip_str = WiFi.softAPIP().toString();
  lv_label_set_text(wifi_ip_label, ip_str.length() ? ip_str.c_str() : "192.168.4.1");
  wifi_update_conn_status_label();
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
  // Statuszeile im WLAN-Popup sofort nachziehen (AP aktiv + IP bzw. wieder
  // Verbunden/Offline) - vorher gab der Toggle dort kein Feedback.
  wifi_update_conn_status_label();
}

void settings_refresh_language() {
  const auto& s = tr();

  if (settings_tile_display_title) lv_label_set_text(settings_tile_display_title, s.display_label);
  if (settings_tile_wifi_title) lv_label_set_text(settings_tile_wifi_title, s.wifi_label);
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

  update_wake_button(mains_wake_label, mains_wake_sub_label, kWakeModeTouch);
  update_wake_button(battery_wake_label, battery_wake_sub_label, kWakeModeTouch);
  settings_update_ap_mode(ap_mode_active);
  update_settings_tile_summaries();

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

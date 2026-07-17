#include "src/ui/climate_popup.h"

#include "src/core/config_manager.h"
#include "src/core/i18n.h"
#include "src/fonts/ui_fonts.h"
#include "src/network/mqtt_handlers.h"
#include "src/tiles/mdi_icons.h"
#include "src/ui/energy_popup.h"
#include "src/ui/light_popup.h"
#include "src/ui/media_popup.h"
#include "src/ui/popup_layout.h"
#include "src/ui/sensor_popup.h"
#include "src/ui/ui_surface_style.h"
#include "src/ui/weather_popup.h"

#include <math.h>
#include <ctype.h>
#include <cstring>

namespace {

constexpr uint8_t kModeButtonCount = 8;
constexpr uint32_t kRemoteBlockMs = 2200;
constexpr uint32_t kTemperatureDebounceMs = 220;
constexpr uint32_t kModeDebounceMs = 160;
constexpr uint32_t kCardBg = 0x2A2A2A;
constexpr uint32_t kControlBg = 0x383838;
constexpr uint32_t kAccent = 0xFF8A3D;
constexpr int kGaugeSize =
    popup_layout::kBodyHeight > 350 ? 338 : popup_layout::kBodyHeight - 20;

struct ClimatePopupContext {
  String entity_id;
  String temperature_unit = "\xC2\xB0" "C";
  String hvac_mode;
  String modes[kModeButtonCount];
  uint8_t mode_count = 0;
  float current_temperature = 0.0f;
  float target_temperature = 20.0f;
  float target_temp_low = 18.0f;
  float target_temp_high = 24.0f;
  float min_temp = 7.0f;
  float max_temp = 35.0f;
  float step = 0.5f;
  bool has_current = false;
  bool has_target = false;
  bool has_range = false;
  bool dragging = false;
  bool suppress_events = false;
  uint32_t block_remote_until_ms = 0;

  lv_obj_t* overlay = nullptr;
  lv_obj_t* card = nullptr;
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* title_label = nullptr;
  lv_obj_t* current_label = nullptr;
  lv_obj_t* arc = nullptr;
  lv_obj_t* target_label = nullptr;
  lv_obj_t* target_caption = nullptr;
  lv_obj_t* minus_button = nullptr;
  lv_obj_t* plus_button = nullptr;
  lv_obj_t* mode_row = nullptr;
  lv_obj_t* mode_buttons[kModeButtonCount] = {};
  lv_obj_t* mode_icons[kModeButtonCount] = {};
  lv_obj_t* mode_labels[kModeButtonCount] = {};
  lv_timer_t* temperature_publish_timer = nullptr;
  lv_timer_t* mode_publish_timer = nullptr;
  String pending_temperature_entity;
  float pending_target_temperature = 20.0f;
  float pending_target_temp_low = 18.0f;
  float pending_target_temp_high = 24.0f;
  bool pending_target_range = false;
  String pending_mode_entity;
  String pending_hvac_mode;
};

ClimatePopupContext* g_climate_popup = nullptr;

bool is_german() {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  return tr.code && strncmp(tr.code, "de", 2) == 0;
}

float clamp_temperature(float value, float min_temp, float max_temp) {
  if (max_temp <= min_temp) {
    min_temp = 7.0f;
    max_temp = 35.0f;
  }
  if (value < min_temp) return min_temp;
  if (value > max_temp) return max_temp;
  return value;
}

float quantize_temperature(float value, float step) {
  if (step <= 0.0f || step > 10.0f) step = 0.5f;
  return roundf(value / step) * step;
}

int arc_value_for(float value) {
  return static_cast<int>(lroundf(value * 10.0f));
}

float temperature_for_arc(int value) {
  return static_cast<float>(value) / 10.0f;
}

const char* mode_icon(const String& mode) {
  if (mode == "off") return "power";
  if (mode == "heat") return "radiator";
  if (mode == "cool") return "snowflake";
  if (mode == "heat_cool") return "thermostat-auto";
  if (mode == "auto") return "thermostat-auto";
  if (mode == "dry") return "water-percent";
  if (mode == "fan_only") return "fan";
  return "thermostat";
}

String mode_label(const String& mode) {
  if (mode == "off") return is_german() ? "Aus" : "Off";
  if (mode == "heat") return is_german() ? "Heizen" : "Heat";
  if (mode == "cool") return is_german() ? "K\xC3\xBChlen" : "Cool";
  if (mode == "heat_cool") return is_german() ? "Auto H/K" : "Heat/Cool";
  if (mode == "auto") return "Auto";
  if (mode == "dry") return is_german() ? "Trocknen" : "Dry";
  if (mode == "fan_only") {
    return is_german() ? "L\xC3\xBC" "fter" : "Fan";
  }
  String label = mode;
  label.replace("_", " ");
  if (label.length()) label.setCharAt(0, toupper(label.charAt(0)));
  return label;
}

uint32_t mode_color(const String& mode) {
  if (mode == "heat") return 0xFF8A3D;
  if (mode == "cool") return 0x4FC3F7;
  if (mode == "dry") return 0xFFD54F;
  if (mode == "fan_only") return 0x4DB6AC;
  if (mode == "off") return 0x9E9E9E;
  return 0xFFFFFF;
}

void set_button_style(lv_obj_t* button, uint32_t color, bool active) {
  if (!button) return;
  lv_obj_set_style_bg_color(
      button, lv_color_hex(active ? color : kControlBg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(button, active ? LV_OPA_COVER : LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_border_width(button, active ? 2 : 0, LV_PART_MAIN);
  lv_obj_set_style_border_color(button, lv_color_hex(color), LV_PART_MAIN);
}

void refresh_mode_buttons(ClimatePopupContext* ctx) {
  if (!ctx) return;
  for (uint8_t i = 0; i < kModeButtonCount; ++i) {
    lv_obj_t* button = ctx->mode_buttons[i];
    if (!button) continue;
    if (i >= ctx->mode_count || !ctx->modes[i].length()) {
      lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    const String& mode = ctx->modes[i];
    lv_obj_clear_flag(button, LV_OBJ_FLAG_HIDDEN);
    if (ctx->mode_icons[i]) {
      const String icon = getMdiChar(mode_icon(mode));
      lv_label_set_text(ctx->mode_icons[i], icon.c_str());
    }
    if (ctx->mode_labels[i]) {
      const String label = mode_label(mode);
      lv_label_set_text(ctx->mode_labels[i], label.c_str());
    }
    const bool active = mode.equalsIgnoreCase(ctx->hvac_mode);
    const uint32_t color = mode_color(mode);
    set_button_style(button, color, active);
    if (ctx->mode_icons[i]) {
      lv_obj_set_style_text_color(
          ctx->mode_icons[i],
          lv_color_hex(active ? 0x111111 : color), 0);
    }
    if (ctx->mode_labels[i]) {
      lv_obj_set_style_text_color(
          ctx->mode_labels[i],
          lv_color_hex(active ? 0x111111 : 0xFFFFFF), 0);
    }
  }
}

void refresh_temperature_labels(ClimatePopupContext* ctx) {
  if (!ctx) return;
  String target;
  if (ctx->has_range) {
    target = String(ctx->target_temp_low, 1);
    target += "\xE2\x80\x93";
    target += String(ctx->target_temp_high, 1);
  } else {
    target = String(ctx->target_temperature, 1);
  }
  target += "\xC2\xB0";
  if (ctx->target_label) lv_label_set_text(ctx->target_label, target.c_str());

  String current = is_german() ? "Ist: " : "Current: ";
  current += ctx->has_current ? String(ctx->current_temperature, 1) : String("--");
  current += " ";
  current += ctx->temperature_unit;
  if (ctx->current_label) lv_label_set_text(ctx->current_label, current.c_str());
}

void refresh_arc(ClimatePopupContext* ctx) {
  if (!ctx || !ctx->arc) return;
  const float center = ctx->has_range
                           ? (ctx->target_temp_low + ctx->target_temp_high) * 0.5f
                           : ctx->target_temperature;
  ctx->suppress_events = true;
  lv_arc_set_range(
      ctx->arc, arc_value_for(ctx->min_temp), arc_value_for(ctx->max_temp));
  lv_arc_set_value(ctx->arc, arc_value_for(center));
  ctx->suppress_events = false;
  refresh_temperature_labels(ctx);
}

void parse_modes(ClimatePopupContext* ctx, String csv) {
  if (!ctx) return;
  ctx->mode_count = 0;
  csv.replace("[", "");
  csv.replace("]", "");
  csv.replace("\"", "");
  csv.replace("'", "");
  int start = 0;
  while (start <= csv.length() && ctx->mode_count < kModeButtonCount) {
    int comma = csv.indexOf(',', start);
    if (comma < 0) comma = csv.length();
    String mode = csv.substring(start, comma);
    mode.trim();
    mode.toLowerCase();
    if (mode.length()) {
      ctx->modes[ctx->mode_count++] = mode;
    }
    if (comma >= csv.length()) break;
    start = comma + 1;
  }
  if (ctx->mode_count == 0 && ctx->hvac_mode.length()) {
    ctx->modes[ctx->mode_count++] = ctx->hvac_mode;
  }
}

void apply_init(ClimatePopupContext* ctx, const ClimatePopupInit& init) {
  if (!ctx) return;
  ctx->entity_id = init.entity_id;
  ctx->hvac_mode = init.hvac_mode;
  ctx->hvac_mode.toLowerCase();
  ctx->temperature_unit =
      init.temperature_unit.length()
          ? init.temperature_unit
          : String("\xC2\xB0" "C");
  ctx->has_current = init.has_current_temperature;
  ctx->has_target = init.has_target_temperature;
  ctx->has_range = init.has_target_range && !init.has_target_temperature;
  ctx->current_temperature = init.current_temperature;
  ctx->target_temperature = init.has_target_temperature
                                ? init.target_temperature
                                : (init.has_target_range
                                       ? (init.target_temp_low + init.target_temp_high) * 0.5f
                                       : 20.0f);
  ctx->target_temp_low = init.target_temp_low;
  ctx->target_temp_high = init.target_temp_high;
  ctx->min_temp = init.min_temp;
  ctx->max_temp = init.max_temp;
  ctx->step = init.target_temp_step;
  if (ctx->max_temp <= ctx->min_temp) {
    ctx->min_temp = 7.0f;
    ctx->max_temp = 35.0f;
  }
  if (ctx->step <= 0.0f || ctx->step > 10.0f) ctx->step = 0.5f;

  if (ctx->title_label) {
    lv_label_set_text(ctx->title_label, init.title.c_str());
  }
  if (ctx->icon_label) {
    const String icon = getMdiChar(
        init.icon_name.length() ? init.icon_name : String("thermostat"));
    lv_label_set_text(ctx->icon_label, icon.c_str());
    lv_obj_set_style_text_color(
        ctx->icon_label, lv_color_hex(mode_color(ctx->hvac_mode)), 0);
  }
  parse_modes(ctx, init.hvac_modes);
  refresh_arc(ctx);
  refresh_mode_buttons(ctx);
}

void publish_pending_temperature(ClimatePopupContext* ctx) {
  if (!ctx || !ctx->pending_temperature_entity.length()) return;
  ctx->block_remote_until_ms = millis() + kRemoteBlockMs;
  mqttPublishClimateTemperature(
      ctx->pending_temperature_entity.c_str(),
      ctx->pending_target_temperature,
      ctx->pending_target_range,
      ctx->pending_target_temp_low,
      ctx->pending_target_temp_high);
  ctx->pending_temperature_entity = "";
}

void temperature_publish_timer_cb(lv_timer_t* timer) {
  ClimatePopupContext* ctx =
      static_cast<ClimatePopupContext*>(lv_timer_get_user_data(timer));
  if (!ctx) return;
  ctx->temperature_publish_timer = nullptr;
  publish_pending_temperature(ctx);
}

void schedule_temperature_publish(ClimatePopupContext* ctx) {
  if (!ctx) return;
  if (ctx->temperature_publish_timer &&
      ctx->pending_temperature_entity.length() &&
      !ctx->pending_temperature_entity.equalsIgnoreCase(ctx->entity_id)) {
    publish_pending_temperature(ctx);
    lv_timer_delete(ctx->temperature_publish_timer);
    ctx->temperature_publish_timer = nullptr;
  }
  ctx->pending_temperature_entity = ctx->entity_id;
  ctx->pending_target_temperature = ctx->target_temperature;
  ctx->pending_target_range = ctx->has_range;
  ctx->pending_target_temp_low = ctx->target_temp_low;
  ctx->pending_target_temp_high = ctx->target_temp_high;
  ctx->block_remote_until_ms = millis() + kRemoteBlockMs;
  if (ctx->temperature_publish_timer) {
    lv_timer_reset(ctx->temperature_publish_timer);
    return;
  }
  ctx->temperature_publish_timer =
      lv_timer_create(temperature_publish_timer_cb, kTemperatureDebounceMs, ctx);
  if (ctx->temperature_publish_timer) {
    lv_timer_set_repeat_count(ctx->temperature_publish_timer, 1);
  }
}

void mode_publish_timer_cb(lv_timer_t* timer) {
  ClimatePopupContext* ctx =
      static_cast<ClimatePopupContext*>(lv_timer_get_user_data(timer));
  if (!ctx) return;
  ctx->mode_publish_timer = nullptr;
  if (!ctx->pending_mode_entity.length() || !ctx->pending_hvac_mode.length()) {
    return;
  }
  mqttPublishClimateHvacMode(
      ctx->pending_mode_entity.c_str(), ctx->pending_hvac_mode.c_str());
  ctx->pending_mode_entity = "";
  ctx->pending_hvac_mode = "";
}

void schedule_mode_publish(ClimatePopupContext* ctx) {
  if (!ctx) return;
  if (ctx->mode_publish_timer && ctx->pending_mode_entity.length() &&
      !ctx->pending_mode_entity.equalsIgnoreCase(ctx->entity_id)) {
    if (ctx->pending_hvac_mode.length()) {
      mqttPublishClimateHvacMode(
          ctx->pending_mode_entity.c_str(), ctx->pending_hvac_mode.c_str());
    }
    ctx->pending_mode_entity = "";
    ctx->pending_hvac_mode = "";
    lv_timer_delete(ctx->mode_publish_timer);
    ctx->mode_publish_timer = nullptr;
  }
  ctx->pending_mode_entity = ctx->entity_id;
  ctx->pending_hvac_mode = ctx->hvac_mode;
  ctx->block_remote_until_ms = millis() + kRemoteBlockMs;
  if (ctx->mode_publish_timer) {
    lv_timer_reset(ctx->mode_publish_timer);
    return;
  }
  ctx->mode_publish_timer =
      lv_timer_create(mode_publish_timer_cb, kModeDebounceMs, ctx);
  if (ctx->mode_publish_timer) {
    lv_timer_set_repeat_count(ctx->mode_publish_timer, 1);
  }
}

void shift_target(ClimatePopupContext* ctx, float delta) {
  if (!ctx) return;
  if (ctx->has_range) {
    const float width = ctx->target_temp_high - ctx->target_temp_low;
    float center =
        quantize_temperature(
            (ctx->target_temp_low + ctx->target_temp_high) * 0.5f + delta,
            ctx->step);
    const float half = width > 0.0f ? width * 0.5f : ctx->step;
    center = clamp_temperature(
        center, ctx->min_temp + half, ctx->max_temp - half);
    ctx->target_temp_low = center - half;
    ctx->target_temp_high = center + half;
  } else {
    ctx->target_temperature = clamp_temperature(
        quantize_temperature(ctx->target_temperature + delta, ctx->step),
        ctx->min_temp, ctx->max_temp);
  }
  refresh_arc(ctx);
  schedule_temperature_publish(ctx);
}

void on_close(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  ClimatePopupContext* ctx =
      static_cast<ClimatePopupContext*>(lv_event_get_user_data(event));
  if (!ctx || !ctx->card || !ctx->overlay) return;
  lv_obj_add_flag(ctx->card, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
}

void on_arc_event(lv_event_t* event) {
  ClimatePopupContext* ctx =
      static_cast<ClimatePopupContext*>(lv_event_get_user_data(event));
  if (!ctx || ctx->suppress_events) return;
  const lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_PRESSED) {
    ctx->dragging = true;
    return;
  }
  if (code == LV_EVENT_VALUE_CHANGED) {
    float value = temperature_for_arc(lv_arc_get_value(ctx->arc));
    value = clamp_temperature(
        quantize_temperature(value, ctx->step), ctx->min_temp, ctx->max_temp);
    if (ctx->has_range) {
      const float width = ctx->target_temp_high - ctx->target_temp_low;
      const float half = width > 0.0f ? width * 0.5f : ctx->step;
      value = clamp_temperature(
          value, ctx->min_temp + half, ctx->max_temp - half);
      ctx->target_temp_low = value - half;
      ctx->target_temp_high = value + half;
    } else {
      ctx->target_temperature = value;
    }
    refresh_temperature_labels(ctx);
    return;
  }
  if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    const bool was_dragging = ctx->dragging;
    ctx->dragging = false;
    if (was_dragging) schedule_temperature_publish(ctx);
  }
}

void on_minus(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  ClimatePopupContext* ctx =
      static_cast<ClimatePopupContext*>(lv_event_get_user_data(event));
  if (!ctx) return;
  shift_target(ctx, -ctx->step);
}

void on_plus(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  ClimatePopupContext* ctx =
      static_cast<ClimatePopupContext*>(lv_event_get_user_data(event));
  if (!ctx) return;
  shift_target(ctx, ctx->step);
}

void on_mode(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  ClimatePopupContext* ctx =
      static_cast<ClimatePopupContext*>(lv_event_get_user_data(event));
  if (!ctx) return;
  lv_obj_t* target =
      static_cast<lv_obj_t*>(lv_event_get_current_target(event));
  for (uint8_t i = 0; i < ctx->mode_count; ++i) {
    if (ctx->mode_buttons[i] != target) continue;
    ctx->hvac_mode = ctx->modes[i];
    ctx->block_remote_until_ms = millis() + kRemoteBlockMs;
    refresh_mode_buttons(ctx);
    schedule_mode_publish(ctx);
    break;
  }
}

lv_obj_t* create_step_button(
    lv_obj_t* parent, const char* icon_name, lv_align_t align, int x,
    ClimatePopupContext* ctx, lv_event_cb_t callback) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_set_size(button, 76, 76);
  lv_obj_align(button, align, x, 0);
  lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(kControlBg), 0);
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, ctx);
  lv_obj_t* label = lv_label_create(button);
  lv_obj_set_style_text_font(label, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  const String icon = getMdiChar(icon_name);
  lv_label_set_text(label, icon.c_str());
  lv_obj_center(label);
  return button;
}

void on_overlay_delete(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_DELETE) return;
  ClimatePopupContext* ctx =
      static_cast<ClimatePopupContext*>(lv_event_get_user_data(event));
  if (g_climate_popup == ctx) g_climate_popup = nullptr;
  if (ctx && ctx->temperature_publish_timer) {
    publish_pending_temperature(ctx);
    lv_timer_delete(ctx->temperature_publish_timer);
    ctx->temperature_publish_timer = nullptr;
  }
  if (ctx && ctx->mode_publish_timer) {
    if (ctx->pending_mode_entity.length() && ctx->pending_hvac_mode.length()) {
      mqttPublishClimateHvacMode(
          ctx->pending_mode_entity.c_str(), ctx->pending_hvac_mode.c_str());
      ctx->pending_mode_entity = "";
      ctx->pending_hvac_mode = "";
    }
    lv_timer_delete(ctx->mode_publish_timer);
    ctx->mode_publish_timer = nullptr;
  }
  delete ctx;
}

}  // namespace

void show_climate_popup(const ClimatePopupInit& init) {
  if (!init.entity_id.length()) return;
  hide_light_popup();
  hide_sensor_popup();
  hide_weather_popup();
  hide_energy_popup();
  hide_media_popup();

  if (g_climate_popup && g_climate_popup->overlay && g_climate_popup->card) {
    apply_init(g_climate_popup, init);
    lv_obj_clear_flag(g_climate_popup->card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_climate_popup->overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(g_climate_popup->overlay);
    return;
  }

  ClimatePopupContext* ctx = new ClimatePopupContext();
  g_climate_popup = ctx;
  ctx->overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(ctx->overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(ctx->overlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ctx->overlay, 0, 0);
  lv_obj_remove_flag(ctx->overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ctx->overlay, LV_OBJ_FLAG_CLICKABLE);

  ctx->card = lv_obj_create(ctx->overlay);
  lv_obj_set_size(
      ctx->card, popup_layout::kCardWidth, popup_layout::kCardHeight);
  lv_obj_center(ctx->card);
  lv_obj_set_style_bg_color(ctx->card, lv_color_hex(kCardBg), 0);
  lv_obj_set_style_bg_opa(ctx->card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ctx->card, 22, 0);
  lv_obj_set_style_border_width(ctx->card, 0, 0);
  ui_surface_style::apply_global_tile_border(ctx->card);
  lv_obj_set_style_pad_all(ctx->card, popup_layout::kCardPad, 0);
  lv_obj_set_style_shadow_width(ctx->card, 28, 0);
  lv_obj_set_style_shadow_color(ctx->card, lv_color_black(), 0);
  lv_obj_set_style_shadow_opa(ctx->card, LV_OPA_40, 0);
  lv_obj_remove_flag(ctx->card, LV_OBJ_FLAG_SCROLLABLE);

  ctx->icon_label = lv_label_create(ctx->card);
  lv_obj_set_style_text_font(ctx->icon_label, FONT_MDI_ICONS, 0);
  lv_obj_align(ctx->icon_label, LV_ALIGN_TOP_LEFT, 2, 18);

  ctx->title_label = lv_label_create(ctx->card);
  lv_obj_set_style_text_font(ctx->title_label, &ui_font_24, 0);
  lv_obj_set_style_text_color(ctx->title_label, lv_color_white(), 0);
  lv_obj_set_width(ctx->title_label, LV_PCT(60));
  lv_label_set_long_mode(ctx->title_label, LV_LABEL_LONG_DOT);
  lv_obj_align(ctx->title_label, LV_ALIGN_TOP_LEFT, 68, 22);

  lv_obj_t* close = lv_button_create(ctx->card);
  lv_obj_set_size(close, 96, 96);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 6, -6);
  lv_obj_set_style_bg_opa(close, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(close, 0, 0);
  lv_obj_set_style_shadow_width(close, 0, 0);
  lv_obj_add_event_cb(close, on_close, LV_EVENT_CLICKED, ctx);
  lv_obj_t* close_icon = lv_label_create(close);
  lv_obj_set_style_text_font(close_icon, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(close_icon, lv_color_white(), 0);
  const String close_char = getMdiChar("window-close");
  lv_label_set_text(close_icon, close_char.c_str());
  lv_obj_center(close_icon);

  ctx->current_label = lv_label_create(ctx->card);
  lv_obj_set_style_text_font(ctx->current_label, &ui_font_24, 0);
  lv_obj_set_style_text_color(ctx->current_label, lv_color_hex(0xD0D0D0), 0);
  lv_obj_set_width(ctx->current_label, LV_PCT(100));
  lv_obj_set_style_text_align(ctx->current_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(
      ctx->current_label, LV_ALIGN_TOP_MID, 0, popup_layout::kValueY + 12);

  lv_obj_t* body = lv_obj_create(ctx->card);
  lv_obj_set_size(body, LV_PCT(100), popup_layout::kBodyHeight);
  lv_obj_align(body, LV_ALIGN_TOP_MID, 0, popup_layout::kBodyY);
  lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(body, 0, 0);
  lv_obj_set_style_pad_all(body, 0, 0);
  lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);

  ctx->arc = lv_arc_create(body);
  lv_obj_set_size(ctx->arc, kGaugeSize, kGaugeSize);
  lv_obj_align(ctx->arc, LV_ALIGN_TOP_MID, 0, -6);
  lv_arc_set_rotation(ctx->arc, 135);
  lv_arc_set_bg_angles(ctx->arc, 0, 270);
  lv_obj_set_style_arc_width(ctx->arc, 20, LV_PART_MAIN);
  lv_obj_set_style_arc_color(ctx->arc, lv_color_hex(0x444444), LV_PART_MAIN);
  lv_obj_set_style_arc_width(ctx->arc, 20, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(ctx->arc, lv_color_hex(kAccent), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(ctx->arc, lv_color_hex(kAccent), LV_PART_KNOB);
  lv_obj_set_style_pad_all(ctx->arc, 7, LV_PART_KNOB);
  lv_obj_add_flag(ctx->arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ctx->arc, on_arc_event, LV_EVENT_PRESSED, ctx);
  lv_obj_add_event_cb(ctx->arc, on_arc_event, LV_EVENT_VALUE_CHANGED, ctx);
  lv_obj_add_event_cb(ctx->arc, on_arc_event, LV_EVENT_RELEASED, ctx);
  lv_obj_add_event_cb(ctx->arc, on_arc_event, LV_EVENT_PRESS_LOST, ctx);

  ctx->target_label = lv_label_create(body);
  lv_obj_set_style_text_font(ctx->target_label, &ui_font_48, 0);
  lv_obj_set_style_text_color(ctx->target_label, lv_color_white(), 0);
  lv_obj_set_width(ctx->target_label, kGaugeSize - 80);
  lv_obj_set_style_text_align(ctx->target_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(ctx->target_label, LV_ALIGN_TOP_MID, 0, 118);

  ctx->target_caption = lv_label_create(body);
  lv_obj_set_style_text_font(ctx->target_caption, &ui_font_20, 0);
  lv_obj_set_style_text_color(ctx->target_caption, lv_color_hex(0xAFAFAF), 0);
  lv_label_set_text(
      ctx->target_caption, is_german() ? "Solltemperatur" : "Target");
  lv_obj_align(ctx->target_caption, LV_ALIGN_TOP_MID, 0, 181);

  ctx->minus_button =
      create_step_button(body, "minus", LV_ALIGN_LEFT_MID, 18, ctx, on_minus);
  ctx->plus_button =
      create_step_button(body, "plus", LV_ALIGN_RIGHT_MID, -18, ctx, on_plus);

  ctx->mode_row = lv_obj_create(ctx->card);
  lv_obj_set_size(
      ctx->mode_row, LV_PCT(100), popup_layout::kNavHeight);
  lv_obj_align(
      ctx->mode_row, LV_ALIGN_BOTTOM_MID, 0, -popup_layout::kNavBottomInset);
  lv_obj_set_style_bg_opa(ctx->mode_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ctx->mode_row, 0, 0);
  lv_obj_set_style_pad_all(ctx->mode_row, 6, 0);
  lv_obj_set_style_pad_column(ctx->mode_row, 10, 0);
  lv_obj_set_layout(ctx->mode_row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(ctx->mode_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(
      ctx->mode_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
      LV_FLEX_ALIGN_CENTER);
  lv_obj_set_scroll_dir(ctx->mode_row, LV_DIR_HOR);
  lv_obj_set_scrollbar_mode(ctx->mode_row, LV_SCROLLBAR_MODE_OFF);

  for (uint8_t i = 0; i < kModeButtonCount; ++i) {
    lv_obj_t* button = lv_button_create(ctx->mode_row);
    ctx->mode_buttons[i] = button;
    lv_obj_set_size(button, 118, 72);
    lv_obj_set_flex_grow(button, 0);
    lv_obj_set_style_radius(button, 20, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_pad_all(button, 5, 0);
    lv_obj_add_event_cb(button, on_mode, LV_EVENT_CLICKED, ctx);

    ctx->mode_icons[i] = lv_label_create(button);
    lv_obj_set_style_text_font(ctx->mode_icons[i], FONT_MDI_ICONS, 0);
    lv_obj_align(ctx->mode_icons[i], LV_ALIGN_TOP_MID, 0, 0);
    ctx->mode_labels[i] = lv_label_create(button);
    lv_obj_set_style_text_font(ctx->mode_labels[i], &ui_font_20, 0);
    lv_obj_set_width(ctx->mode_labels[i], LV_PCT(100));
    lv_obj_set_style_text_align(ctx->mode_labels[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ctx->mode_labels[i], LV_ALIGN_BOTTOM_MID, 0, 2);
  }

  lv_obj_add_event_cb(
      ctx->overlay, on_overlay_delete, LV_EVENT_DELETE, ctx);
  apply_init(ctx, init);
  lv_obj_move_foreground(ctx->overlay);
}

void update_climate_popup(const ClimatePopupInit& init) {
  ClimatePopupContext* ctx = g_climate_popup;
  if (!ctx || !ctx->card || !ctx->overlay) return;
  if (!ctx->entity_id.equalsIgnoreCase(init.entity_id)) return;
  if (lv_obj_has_flag(ctx->card, LV_OBJ_FLAG_HIDDEN)) return;
  if (ctx->dragging || millis() < ctx->block_remote_until_ms) return;
  apply_init(ctx, init);
}

void preload_climate_popup() {
  if (g_climate_popup && g_climate_popup->card) return;
  ClimatePopupInit init;
  init.entity_id = "__preload__";
  init.title = "";
  init.icon_name = "thermostat";
  init.hvac_mode = "off";
  init.hvac_modes = "off,heat,cool,auto";
  init.temperature_unit = "\xC2\xB0" "C";
  init.has_current_temperature = true;
  init.current_temperature = 20.0f;
  init.has_target_temperature = true;
  init.target_temperature = 21.0f;
  show_climate_popup(init);
  hide_climate_popup();
}

void hide_climate_popup() {
  if (!g_climate_popup || !g_climate_popup->card ||
      !g_climate_popup->overlay) return;
  lv_obj_add_flag(g_climate_popup->card, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_climate_popup->overlay, LV_OBJ_FLAG_CLICKABLE);
}

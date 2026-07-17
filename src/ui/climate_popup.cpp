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
constexpr uint32_t kAccent = 0xFF8A3D;
constexpr uint32_t kControlButtonIndicatorBg = 0xFFFFFF;
constexpr lv_opa_t kControlButtonIndicatorOpa = LV_OPA_20;
constexpr int kGaugeSize = 340;
constexpr int kControlButtonSize = 92;
constexpr int kControlsRowGap = 12;
constexpr int kControlsRowPadX = 12;

struct ClimatePopupContext {
  String entity_id;
  String icon_name;
  String temperature_unit = "\xC2\xB0" "C";
  String hvac_mode;
  String hvac_action;
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
  bool icon_visible = true;
  bool dynamic_icon = true;
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
  lv_timer_t* temperature_publish_timer = nullptr;
  lv_timer_t* mode_publish_timer = nullptr;
  lv_timer_t* remote_apply_timer = nullptr;
  ClimatePopupInit pending_remote_init;
  bool has_pending_remote = false;
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

uint32_t mode_color(const String& mode) {
  if (mode == "heat") return 0xFF8A3D;
  if (mode == "cool") return 0x4FC3F7;
  if (mode == "dry") return 0xFFD54F;
  if (mode == "fan_only") return 0x4DB6AC;
  if (mode == "off") return 0x9E9E9E;
  return 0xFFFFFF;
}

const char* dynamic_state_icon(const String& mode, const String& action) {
  if (action == "heating") return "radiator";
  if (action == "cooling") return "snowflake";
  if (action == "drying") return "water-percent";
  if (action == "fan") return "fan";
  if (mode == "off") return "thermometer-off";
  if (action == "idle" || action == "off") return "thermostat";
  return mode_icon(mode);
}

uint32_t state_color(const String& mode, const String& action) {
  if (action == "heating") return 0xFF8A3D;
  if (action == "cooling") return 0x4FC3F7;
  if (action == "drying") return 0xFFD54F;
  if (action == "fan") return 0x4DB6AC;
  if (mode == "off" || action == "idle" || action == "off") return 0x9E9E9E;
  // Manche Thermostate liefern kein hvac_action. Dann ist der eingestellte
  // Modus die beste verfuegbare Information statt eines farblosen Icons.
  return action.length() ? 0xFFFFFF : mode_color(mode);
}

void style_round_icon_button(
    lv_obj_t* button, lv_obj_t* icon, bool active) {
  if (!button) return;
  auto apply_selector = [&](lv_style_selector_t selector, bool pressed) {
    lv_obj_set_style_bg_color(
        button, lv_color_hex(kControlButtonIndicatorBg), selector);
    lv_obj_set_style_bg_opa(
        button,
        (active || pressed) ? kControlButtonIndicatorOpa : LV_OPA_TRANSP,
        selector);
    lv_obj_set_style_border_width(button, 0, selector);
    lv_obj_set_style_border_opa(button, LV_OPA_TRANSP, selector);
    lv_obj_set_style_outline_width(button, 0, selector);
    lv_obj_set_style_shadow_width(button, 0, selector);
    lv_obj_set_style_shadow_opa(button, LV_OPA_TRANSP, selector);
    lv_obj_set_style_anim_time(button, 0, selector);
    lv_obj_set_style_transform_width(button, 0, selector);
    lv_obj_set_style_transform_height(button, 0, selector);
    lv_obj_set_style_translate_y(button, 0, selector);
  };
  apply_selector(0, false);
  apply_selector(LV_STATE_PRESSED, true);
  if (icon) {
    lv_obj_set_style_text_color(icon, lv_color_white(), 0);
    lv_obj_set_style_text_color(icon, lv_color_white(), LV_STATE_PRESSED);
  }
}

void refresh_header_visuals(ClimatePopupContext* ctx) {
  if (!ctx || !ctx->icon_label) return;
  if (!ctx->icon_visible) {
    lv_obj_add_flag(ctx->icon_label, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  String icon_name = ctx->dynamic_icon
                         ? String(dynamic_state_icon(
                               ctx->hvac_mode, ctx->hvac_action))
                         : ctx->icon_name;
  String icon = getMdiChar(icon_name);
  if (!icon.length()) {
    icon = getMdiChar("thermostat");
  }
  lv_label_set_text(ctx->icon_label, icon.c_str());
  lv_obj_set_style_text_color(
      ctx->icon_label,
      lv_color_hex(state_color(ctx->hvac_mode, ctx->hvac_action)),
      0);
  lv_obj_clear_flag(ctx->icon_label, LV_OBJ_FLAG_HIDDEN);
}

void refresh_control_accent(ClimatePopupContext* ctx) {
  if (!ctx || !ctx->arc) return;
  const uint32_t accent = mode_color(ctx->hvac_mode);
  lv_obj_set_style_arc_color(
      ctx->arc, lv_color_hex(accent), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(
      ctx->arc, lv_color_hex(accent), LV_PART_KNOB);
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
    const bool active = mode.equalsIgnoreCase(ctx->hvac_mode);
    style_round_icon_button(button, ctx->mode_icons[i], active);
  }
  if (ctx->mode_row) {
    const int content_width =
        static_cast<int>(ctx->mode_count) * kControlButtonSize +
        static_cast<int>(ctx->mode_count > 0 ? ctx->mode_count - 1 : 0) *
            kControlsRowGap +
        (kControlsRowPadX * 2);
    lv_obj_set_flex_align(
        ctx->mode_row,
        content_width <= popup_layout::kContentWidth
            ? LV_FLEX_ALIGN_CENTER
            : LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER);
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
  target += " ";
  target += ctx->temperature_unit;
  if (ctx->target_label) lv_label_set_text(ctx->target_label, target.c_str());

  String current =
      ctx->has_current ? String(ctx->current_temperature, 1) : String("--");
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
  ctx->icon_name = init.icon_name;
  ctx->icon_visible = init.icon_visible;
  ctx->dynamic_icon = init.dynamic_icon;
  ctx->hvac_mode = init.hvac_mode;
  ctx->hvac_mode.toLowerCase();
  ctx->hvac_action = init.hvac_action;
  ctx->hvac_action.toLowerCase();
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
  parse_modes(ctx, init.hvac_modes);
  refresh_header_visuals(ctx);
  refresh_control_accent(ctx);
  refresh_arc(ctx);
  refresh_mode_buttons(ctx);
}

void cancel_deferred_remote_apply(ClimatePopupContext* ctx) {
  if (!ctx) return;
  if (ctx->remote_apply_timer) {
    lv_timer_delete(ctx->remote_apply_timer);
    ctx->remote_apply_timer = nullptr;
  }
  ctx->has_pending_remote = false;
}

void remote_apply_timer_cb(lv_timer_t* timer) {
  ClimatePopupContext* ctx =
      static_cast<ClimatePopupContext*>(lv_timer_get_user_data(timer));
  if (!ctx) {
    lv_timer_delete(timer);
    return;
  }
  if (ctx->dragging || millis() < ctx->block_remote_until_ms) {
    const uint32_t now = millis();
    const uint32_t remaining =
        ctx->block_remote_until_ms > now
            ? (ctx->block_remote_until_ms - now + 20)
            : 50;
    lv_timer_set_period(timer, remaining);
    lv_timer_reset(timer);
    return;
  }

  ctx->remote_apply_timer = nullptr;
  lv_timer_delete(timer);
  if (!ctx->has_pending_remote) return;
  ClimatePopupInit pending = ctx->pending_remote_init;
  ctx->has_pending_remote = false;
  if (!ctx->card || lv_obj_has_flag(ctx->card, LV_OBJ_FLAG_HIDDEN) ||
      !ctx->entity_id.equalsIgnoreCase(pending.entity_id)) {
    return;
  }
  apply_init(ctx, pending);
}

void defer_remote_apply(
    ClimatePopupContext* ctx, const ClimatePopupInit& init) {
  if (!ctx) return;
  ctx->pending_remote_init = init;
  ctx->has_pending_remote = true;
  const uint32_t now = millis();
  const uint32_t delay_ms =
      ctx->block_remote_until_ms > now
          ? (ctx->block_remote_until_ms - now + 20)
          : 50;
  if (ctx->remote_apply_timer) {
    lv_timer_set_period(ctx->remote_apply_timer, delay_ms);
    lv_timer_reset(ctx->remote_apply_timer);
    return;
  }
  ctx->remote_apply_timer =
      lv_timer_create(remote_apply_timer_cb, delay_ms, ctx);
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
  const lv_event_code_t code = lv_event_get_code(event);
  if (code != LV_EVENT_CLICKED && code != LV_EVENT_RELEASED) return;
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
    // Die bisherige Aktion gehoert noch zum alten Modus. Bis zur HA-
    // Rueckmeldung den neuen Modus sichtbar darstellen, statt z.B. nach
    // "Heat" weiterhin ein graues Off-/Idle-Icon zu zeigen.
    ctx->hvac_action = "";
    ctx->block_remote_until_ms = millis() + kRemoteBlockMs;
    refresh_header_visuals(ctx);
    refresh_control_accent(ctx);
    refresh_mode_buttons(ctx);
    schedule_mode_publish(ctx);
    break;
  }
}

void align_header_row(
    lv_obj_t* card, lv_obj_t* title_label, lv_obj_t* icon_label) {
  if (!card) return;
  lv_obj_update_layout(card);
  lv_coord_t header_center_y =
      60 - lv_obj_get_style_pad_top(card, LV_PART_MAIN);
  if (header_center_y < 0) header_center_y = 0;
  if (icon_label) {
    lv_coord_t icon_y =
        header_center_y - (lv_obj_get_height(icon_label) / 2);
    if (icon_y < 0) icon_y = 0;
    lv_obj_align(icon_label, LV_ALIGN_TOP_LEFT, 8, icon_y);
  }
  if (title_label) {
    lv_coord_t title_y =
        header_center_y - (lv_obj_get_height(title_label) / 2);
    if (title_y < 0) title_y = 0;
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 78, title_y);
  }
}

lv_obj_t* create_step_button(
    lv_obj_t* parent, const char* icon_name, lv_align_t align, int x,
    ClimatePopupContext* ctx, lv_event_cb_t callback) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_set_size(button, kControlButtonSize, kControlButtonSize);
  lv_obj_align(button, align, x, 0);
  lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_pad_all(button, 0, 0);
  lv_obj_add_flag(button, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_set_ext_click_area(button, 10);
  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, ctx);
  lv_obj_t* label = lv_label_create(button);
  lv_obj_set_style_text_font(label, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  const String icon = getMdiChar(icon_name);
  lv_label_set_text(label, icon.c_str());
  lv_obj_center(label);
  style_round_icon_button(button, label, false);
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
  if (ctx && ctx->remote_apply_timer) {
    lv_timer_delete(ctx->remote_apply_timer);
    ctx->remote_apply_timer = nullptr;
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
    cancel_deferred_remote_apply(g_climate_popup);
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
  lv_obj_set_style_shadow_spread(ctx->card, 2, 0);
  lv_obj_remove_flag(ctx->card, LV_OBJ_FLAG_SCROLLABLE);

  ctx->icon_label = lv_label_create(ctx->card);
  lv_obj_set_style_text_font(ctx->icon_label, FONT_MDI_ICONS, 0);

  ctx->title_label = lv_label_create(ctx->card);
  lv_obj_set_style_text_font(ctx->title_label, &ui_font_24, 0);
  lv_obj_set_style_text_color(ctx->title_label, lv_color_white(), 0);
  lv_obj_set_width(ctx->title_label, LV_PCT(62));
  lv_label_set_long_mode(ctx->title_label, LV_LABEL_LONG_DOT);

  lv_obj_t* close = lv_button_create(ctx->card);
  lv_obj_set_size(close, 96, 96);
  lv_obj_set_style_bg_opa(close, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(
      close, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(close, LV_OPA_20, LV_STATE_PRESSED);
  lv_obj_set_style_border_opa(close, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(close, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(close, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(close, 16, 0);
  lv_obj_set_style_pad_all(close, 0, 0);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 6, -6);
  lv_obj_set_ext_click_area(close, 8);
  lv_obj_add_flag(close, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_clear_flag(close, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(close, on_close, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(close, on_close, LV_EVENT_RELEASED, ctx);
  lv_obj_t* close_icon = lv_label_create(close);
  lv_obj_set_style_text_font(close_icon, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(close_icon, lv_color_white(), 0);
  const String close_char = getMdiChar("window-close");
  lv_label_set_text(close_icon, close_char.c_str());
  lv_obj_center(close_icon);

  lv_obj_t* value_box = lv_obj_create(ctx->card);
  lv_obj_remove_style_all(value_box);
  lv_obj_set_size(value_box, LV_PCT(100), popup_layout::kValueHeight);
  lv_obj_align(value_box, LV_ALIGN_TOP_MID, 0, popup_layout::kValueY);
  lv_obj_set_style_bg_opa(value_box, LV_OPA_TRANSP, 0);
  lv_obj_set_layout(value_box, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(value_box, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(
      value_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
      LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(value_box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(value_box, LV_OBJ_FLAG_SCROLLABLE);

  ctx->current_label = lv_label_create(value_box);
  lv_obj_set_style_text_font(ctx->current_label, &ui_font_48, 0);
  lv_obj_set_style_text_color(ctx->current_label, lv_color_white(), 0);
  lv_obj_set_width(ctx->current_label, LV_PCT(100));
  lv_obj_set_height(ctx->current_label, popup_layout::kValueHeight);
  lv_obj_set_style_text_align(ctx->current_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_translate_y(
      ctx->current_label, popup_layout::kLargeValueTextOffsetY, 0);

  lv_obj_t* body = lv_obj_create(ctx->card);
  lv_obj_set_size(body, LV_PCT(100), popup_layout::kBodyHeight);
  lv_obj_align(body, LV_ALIGN_TOP_MID, 0, popup_layout::kBodyY);
  lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(body, 0, 0);
  lv_obj_set_style_pad_all(body, 0, 0);
  lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(body, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  ctx->arc = lv_arc_create(body);
  lv_obj_set_size(ctx->arc, kGaugeSize, kGaugeSize);
  lv_obj_align(ctx->arc, LV_ALIGN_CENTER, 0, -6);
  lv_arc_set_rotation(ctx->arc, 135);
  lv_arc_set_bg_angles(ctx->arc, 0, 270);
  lv_obj_set_style_arc_width(ctx->arc, 20, LV_PART_MAIN);
  lv_obj_set_style_arc_color(ctx->arc, lv_color_hex(0x444444), LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(ctx->arc, true, LV_PART_MAIN);
  lv_obj_set_style_arc_width(ctx->arc, 20, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(ctx->arc, lv_color_hex(kAccent), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(ctx->arc, true, LV_PART_INDICATOR);
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
  lv_obj_align(ctx->target_label, LV_ALIGN_CENTER, 0, -14);

  ctx->target_caption = lv_label_create(body);
  lv_obj_set_style_text_font(ctx->target_caption, &ui_font_20, 0);
  lv_obj_set_style_text_color(ctx->target_caption, lv_color_hex(0xAFAFAF), 0);
  lv_label_set_text(
      ctx->target_caption, is_german() ? "Solltemperatur" : "Target");
  lv_obj_align(ctx->target_caption, LV_ALIGN_CENTER, 0, 38);

  ctx->minus_button =
      create_step_button(body, "minus", LV_ALIGN_LEFT_MID, 18, ctx, on_minus);
  ctx->plus_button =
      create_step_button(body, "plus", LV_ALIGN_RIGHT_MID, -18, ctx, on_plus);

  ctx->mode_row = lv_obj_create(ctx->card);
  lv_obj_set_size(ctx->mode_row, LV_PCT(100), popup_layout::kNavHeight);
  lv_obj_align(
      ctx->mode_row, LV_ALIGN_BOTTOM_MID, 0, -popup_layout::kNavBottomInset);
  lv_obj_set_style_bg_opa(ctx->mode_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(ctx->mode_row, 0, 0);
  lv_obj_set_style_border_width(ctx->mode_row, 0, 0);
  lv_obj_set_style_pad_left(ctx->mode_row, kControlsRowPadX, 0);
  lv_obj_set_style_pad_right(ctx->mode_row, kControlsRowPadX, 0);
  lv_obj_set_style_pad_top(ctx->mode_row, 0, 0);
  lv_obj_set_style_pad_bottom(ctx->mode_row, 0, 0);
  lv_obj_set_style_pad_column(ctx->mode_row, kControlsRowGap, 0);
  lv_obj_set_layout(ctx->mode_row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(ctx->mode_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(
      ctx->mode_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
      LV_FLEX_ALIGN_CENTER);
  lv_obj_set_scroll_dir(ctx->mode_row, LV_DIR_HOR);
  lv_obj_set_scrollbar_mode(ctx->mode_row, LV_SCROLLBAR_MODE_OFF);

  for (uint8_t i = 0; i < kModeButtonCount; ++i) {
    lv_obj_t* button = lv_button_create(ctx->mode_row);
    ctx->mode_buttons[i] = button;
    lv_obj_set_size(button, kControlButtonSize, kControlButtonSize);
    lv_obj_set_flex_grow(button, 0);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_add_flag(button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_ext_click_area(button, 10);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(button, on_mode, LV_EVENT_CLICKED, ctx);

    ctx->mode_icons[i] = lv_label_create(button);
    lv_obj_set_style_text_font(ctx->mode_icons[i], FONT_MDI_ICONS, 0);
    lv_obj_center(ctx->mode_icons[i]);
    style_round_icon_button(button, ctx->mode_icons[i], false);
  }

  lv_obj_add_event_cb(
      ctx->overlay, on_overlay_delete, LV_EVENT_DELETE, ctx);
  align_header_row(ctx->card, ctx->title_label, ctx->icon_label);
  lv_obj_move_foreground(ctx->icon_label);
  lv_obj_move_foreground(ctx->title_label);
  lv_obj_move_foreground(close);
  apply_init(ctx, init);
  lv_obj_move_foreground(ctx->overlay);
}

void update_climate_popup(const ClimatePopupInit& init) {
  ClimatePopupContext* ctx = g_climate_popup;
  if (!ctx || !ctx->card || !ctx->overlay) return;
  if (!ctx->entity_id.equalsIgnoreCase(init.entity_id)) return;
  if (lv_obj_has_flag(ctx->card, LV_OBJ_FLAG_HIDDEN)) return;
  if (ctx->dragging || millis() < ctx->block_remote_until_ms) {
    defer_remote_apply(ctx, init);
    return;
  }
  cancel_deferred_remote_apply(ctx);
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

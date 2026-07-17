#include "src/types/climate/renderer.h"

#include "src/network/ha_bridge_config.h"
#include "src/tiles/mdi_icons.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/ui/climate_popup.h"

namespace {

struct ClimateEventData {
  GridType grid_type = GridType::TAB0;
  uint8_t index = 0;
};

ClimatePopupInit popup_init_for(const ClimateEventData* data) {
  ClimatePopupInit init;
  if (!data || data->index >= TILES_PER_GRID) return init;
  const Tile* tile = tile_renderer_get_tile_config(data->grid_type, data->index);
  ClimateState* states = tile_renderer_get_climate_states(data->grid_type);
  if (!tile || !states) return init;
  const ClimateState& state = states[data->index];

  init.entity_id = tile->sensor_entity;
  init.title = tile->title.length()
                   ? tile->title
                   : haBridgeConfig.findSensorName(tile->sensor_entity);
  if (!init.title.length()) init.title = tile->sensor_entity;
  String configured_icon = normalizeMdiIconName(tile->icon_name);
  if (configured_icon.length()) {
    init.icon_name = configured_icon;
  } else if (strcmp(state.hvac_action, "heating") == 0) {
    init.icon_name = "radiator";
  } else if (strcmp(state.hvac_action, "cooling") == 0) {
    init.icon_name = "snowflake";
  } else if (strcmp(state.hvac_action, "drying") == 0) {
    init.icon_name = "water-percent";
  } else if (strcmp(state.hvac_action, "fan") == 0) {
    init.icon_name = "fan";
  } else if (state.hvac_action[0]) {
    init.icon_name =
        strcmp(state.hvac_mode, "off") == 0 ? "thermometer-off" : "thermostat";
  } else if (strcmp(state.hvac_mode, "heat") == 0) {
    init.icon_name = "radiator";
  } else if (strcmp(state.hvac_mode, "cool") == 0) {
    init.icon_name = "snowflake";
  } else {
    init.icon_name = "thermostat";
  }
  init.hvac_mode = state.hvac_mode;
  init.hvac_action = state.hvac_action;
  init.hvac_modes = climateHvacModesCsv(state.hvac_modes_mask);
  init.temperature_unit = state.temperature_unit;
  init.current_temperature = state.current_temperature;
  init.target_temperature = state.target_temperature;
  init.target_temp_low = state.target_temp_low;
  init.target_temp_high = state.target_temp_high;
  init.min_temp = state.min_temp;
  init.max_temp = state.max_temp;
  init.target_temp_step = state.target_temp_step;
  init.has_current_temperature = state.has_current_temperature;
  init.has_target_temperature = state.has_target_temperature;
  init.has_target_range = state.has_target_range;
  return init;
}

}  // namespace

lv_obj_t* render_climate_tile(lv_obj_t* parent,
                              int col,
                              int row,
                              const Tile& tile,
                              uint8_t index,
                              GridType grid_type) {
  if (!parent) return nullptr;

  lv_obj_t* card = lv_button_create(parent);
  const uint32_t color = tileBgColorOrDefault(tile, 0x2A2A2A);
  lv_obj_set_style_bg_color(card, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(card, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(
      card, lv_color_hex(brighten_rgb_color(color, 0x10)),
      LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_hor(card, 20, 0);
  lv_obj_set_style_pad_ver(card, 24, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  disable_pressed_button_animation(card);
  set_tile_grid_cell(card, col, row, tile.span_w, tile.span_h);

  const bool icon_disabled = isMdiIconDisabled(tile.icon_name);
  const String configured_icon = normalizeMdiIconName(tile.icon_name);
  const bool dynamic_icon = !icon_disabled && !configured_icon.length();
  String icon_name = configured_icon;
  if (dynamic_icon) icon_name = "thermostat";

  lv_obj_t* icon_label = nullptr;
  const String icon_char = getMdiChar(icon_name);
  if (!icon_disabled && icon_char.length() && FONT_MDI_ICONS) {
    icon_label = lv_label_create(card);
    set_label_style(icon_label, lv_color_white(), FONT_MDI_ICONS);
    lv_label_set_text(icon_label, icon_char.c_str());
    lv_obj_align(icon_label, LV_ALIGN_TOP_LEFT, -8, -8);
  }

  if (tile.title.length()) {
    lv_obj_t* title = lv_label_create(card);
    set_label_style(title, lv_color_white(), FONT_TITLE);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, LV_PCT(70));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(title, tile.title.c_str());
    lv_obj_align(title, LV_ALIGN_TOP_RIGHT, 4, 4);
  }

  lv_obj_t* value = lv_label_create(card);
  set_label_style(value, lv_color_white(), FONT_VALUE);
  lv_obj_set_width(value, LV_PCT(100));
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(value, "-- \xC2\xB0" "C");
  lv_obj_align(value, LV_ALIGN_CENTER, 0, 28);

  ClimateTileWidgets* widgets = tile_renderer_get_climate_widgets(grid_type);
  if (widgets && index < TILES_PER_GRID) {
    widgets[index].icon_label = icon_label;
    widgets[index].value_label = value;
    widgets[index].dynamic_icon = dynamic_icon;
  }

  if (grid_type != GridType::SCREENSAVER && tile.sensor_entity.length()) {
    ClimateEventData* data = new ClimateEventData{grid_type, index};
    const lv_event_code_t popup_event =
        getTilePopupOpenMode(tile) == TILE_POPUP_OPEN_SHORT_PRESS
            ? LV_EVENT_SHORT_CLICKED
            : LV_EVENT_LONG_PRESSED;
    lv_obj_add_event_cb(
        card,
        [](lv_event_t* event) {
          const lv_event_code_t code = lv_event_get_code(event);
          if (code != LV_EVENT_SHORT_CLICKED && code != LV_EVENT_LONG_PRESSED) return;
          ClimateEventData* data =
              static_cast<ClimateEventData*>(lv_event_get_user_data(event));
          ClimatePopupInit init = popup_init_for(data);
          if (!init.entity_id.length()) return;
          finish_press_before_popup(event);
          show_climate_popup(init);
        },
        popup_event,
        data);
    lv_obj_add_event_cb(
        card,
        [](lv_event_t* event) {
          if (lv_event_get_code(event) != LV_EVENT_DELETE) return;
          delete static_cast<ClimateEventData*>(lv_event_get_user_data(event));
        },
        LV_EVENT_DELETE,
        data);
  }

  return card;
}

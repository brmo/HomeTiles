#include "src/types/climate/renderer.h"

#include <algorithm>
#include <cmath>

#include "src/core/config_manager.h"
#include "src/core/i18n.h"
#include "src/network/ha_bridge_config.h"
#include "src/network/mqtt_handlers.h"
#include "src/tiles/mdi_icons.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/ui/climate_popup.h"

namespace {

struct ClimateEventData {
  GridType grid_type = GridType::TAB0;
  uint8_t index = 0;
};

enum class ClimateTileSlotKind : uint8_t {
  NONE = 0,
  CURRENT_TEMPERATURE,
  CURRENT_HUMIDITY,
  TARGET_TEMPERATURE,
  TARGET_TEMPERATURE_LOW,
  TARGET_TEMPERATURE_HIGH,
  TARGET_HUMIDITY,
  HVAC_MODE
};

struct ClimateAdjustEventData {
  GridType grid_type = GridType::TAB0;
  uint8_t index = 0;
  uint8_t slot_index = 0;
  int8_t direction = 0;
};

bool slot_is_adjustable(ClimateTileSlotKind kind) {
  return kind == ClimateTileSlotKind::TARGET_TEMPERATURE ||
         kind == ClimateTileSlotKind::TARGET_TEMPERATURE_LOW ||
         kind == ClimateTileSlotKind::TARGET_TEMPERATURE_HIGH ||
         kind == ClimateTileSlotKind::TARGET_HUMIDITY;
}

uint8_t slot_unit_cost(ClimateTileSlotKind kind) {
  return slot_is_adjustable(kind) ? 2 : 1;
}

float clamp_value(float value, float minimum, float maximum) {
  return std::max(minimum, std::min(value, maximum));
}

float snap_value(float value, float step) {
  if (step <= 0.0f) return value;
  return roundf(value / step) * step;
}

String climate_temperature_text(const ClimateState& state, float value) {
  String text = i18n::format_number(
      configManager.getConfig().language, value, 1);
  text += " ";
  text += state.temperature_unit[0]
              ? state.temperature_unit
              : "\xC2\xB0"
                "C";
  return text;
}

String climate_slot_text(
    ClimateTileSlotKind kind, const ClimateState& state) {
  switch (kind) {
    case ClimateTileSlotKind::CURRENT_TEMPERATURE:
      return state.has_current_temperature
                 ? climate_temperature_text(state, state.current_temperature)
                 : String("-- \xC2\xB0"
                          "C");
    case ClimateTileSlotKind::CURRENT_HUMIDITY:
      if (!state.has_current_humidity) return "--%";
      return i18n::format_number(
                 configManager.getConfig().language,
                 state.current_humidity,
                 0) +
             "%";
    case ClimateTileSlotKind::TARGET_TEMPERATURE:
      return state.has_target_temperature
                 ? climate_temperature_text(state, state.target_temperature)
                 : String("-- \xC2\xB0"
                          "C");
    case ClimateTileSlotKind::TARGET_TEMPERATURE_LOW:
      return state.has_target_range
                 ? climate_temperature_text(state, state.target_temp_low)
                 : String("-- \xC2\xB0"
                          "C");
    case ClimateTileSlotKind::TARGET_TEMPERATURE_HIGH:
      return state.has_target_range
                 ? climate_temperature_text(state, state.target_temp_high)
                 : String("-- \xC2\xB0"
                          "C");
    case ClimateTileSlotKind::TARGET_HUMIDITY:
      if (!state.has_target_humidity) return "--%";
      return i18n::format_number(
                 configManager.getConfig().language,
                 state.target_humidity,
                 0) +
             "%";
    case ClimateTileSlotKind::HVAC_MODE:
      if (!state.hvac_mode[0]) return "--";
      return i18n::climate_option_label(
          configManager.getConfig().language, String(state.hvac_mode));
    default:
      return "";
  }
}

String climate_slot_caption(
    ClimateTileSlotKind kind, const ClimateState& state) {
  const char* language = configManager.getConfig().language;
  if (kind == ClimateTileSlotKind::TARGET_HUMIDITY) {
    return i18n::climate_target_humidity_label(language);
  }
  if (kind == ClimateTileSlotKind::TARGET_TEMPERATURE_LOW) {
    return i18n::climate_state_label(language, "", "heating");
  }
  if (kind == ClimateTileSlotKind::TARGET_TEMPERATURE_HIGH) {
    return i18n::climate_state_label(language, "", "cooling");
  }
  if (kind == ClimateTileSlotKind::TARGET_TEMPERATURE) {
    String action = state.hvac_action;
    action.toLowerCase();
    if (action == "heating" || action == "preheating" ||
        action == "cooling") {
      return i18n::climate_state_label(
          language, state.hvac_mode, state.hvac_action);
    }
    String mode = state.hvac_mode;
    mode.toLowerCase();
    if (mode == "heat") {
      return i18n::climate_state_label(language, "", "heating");
    }
    if (mode == "cool") {
      return i18n::climate_state_label(language, "", "cooling");
    }
    if (mode.length()) {
      return i18n::climate_state_label(language, mode, "");
    }
    return i18n::climate_target_temperature_label(language);
  }
  return "";
}

ClimateTileSlotKind configured_slot_kind(ClimateTileContent content) {
  switch (content) {
    case CLIMATE_TILE_CONTENT_CURRENT_TEMPERATURE:
      return ClimateTileSlotKind::CURRENT_TEMPERATURE;
    case CLIMATE_TILE_CONTENT_CURRENT_HUMIDITY:
      return ClimateTileSlotKind::CURRENT_HUMIDITY;
    case CLIMATE_TILE_CONTENT_TARGET_TEMPERATURE:
      return ClimateTileSlotKind::TARGET_TEMPERATURE;
    case CLIMATE_TILE_CONTENT_TARGET_TEMPERATURE_LOW:
      return ClimateTileSlotKind::TARGET_TEMPERATURE_LOW;
    case CLIMATE_TILE_CONTENT_TARGET_TEMPERATURE_HIGH:
      return ClimateTileSlotKind::TARGET_TEMPERATURE_HIGH;
    case CLIMATE_TILE_CONTENT_TARGET_HUMIDITY:
      return ClimateTileSlotKind::TARGET_HUMIDITY;
    case CLIMATE_TILE_CONTENT_HVAC_MODE:
      return ClimateTileSlotKind::HVAC_MODE;
    default:
      return ClimateTileSlotKind::NONE;
  }
}

uint8_t build_automatic_slot_kinds(
    const Tile& tile, const ClimateState& state,
    ClimateTileSlotKind* out, uint8_t capacity) {
  if (!out || capacity == 0) return 0;
  uint8_t count = 0;
  uint8_t used_units = 0;
  auto append = [&](ClimateTileSlotKind kind) {
    const uint8_t cost = slot_unit_cost(kind);
    if (count < capacity && used_units + cost <= capacity) {
      out[count++] = kind;
      used_units += cost;
    }
  };

  const uint8_t span_w = std::max<uint8_t>(1, tile.span_w);
  const uint8_t span_h = std::max<uint8_t>(1, tile.span_h);

  if (span_w == 1 && span_h == 1) {
    append(ClimateTileSlotKind::CURRENT_TEMPERATURE);
    return count;
  }

  if (span_w >= 2 && span_h == 1) {
    if (state.has_target_range) {
      append(ClimateTileSlotKind::TARGET_TEMPERATURE_LOW);
    } else if (state.has_target_temperature) {
      append(ClimateTileSlotKind::TARGET_TEMPERATURE);
    } else if (state.has_target_humidity) {
      append(ClimateTileSlotKind::TARGET_HUMIDITY);
    } else {
      append(ClimateTileSlotKind::CURRENT_TEMPERATURE);
    }
    return count;
  }

  if (span_w == 1) {
    append(ClimateTileSlotKind::CURRENT_TEMPERATURE);
    if (state.has_target_range) {
      append(ClimateTileSlotKind::TARGET_TEMPERATURE_LOW);
    } else if (state.has_target_temperature) {
      append(ClimateTileSlotKind::TARGET_TEMPERATURE);
    }
    if (state.has_target_humidity) {
      append(ClimateTileSlotKind::TARGET_HUMIDITY);
    }
    return count;
  }

  append(ClimateTileSlotKind::CURRENT_TEMPERATURE);
  if (state.has_current_humidity) {
    append(ClimateTileSlotKind::CURRENT_HUMIDITY);
  }
  if (state.has_target_range) {
    append(ClimateTileSlotKind::TARGET_TEMPERATURE_LOW);
    append(ClimateTileSlotKind::TARGET_TEMPERATURE_HIGH);
  } else if (state.has_target_temperature) {
    append(ClimateTileSlotKind::TARGET_TEMPERATURE);
  }
  if (state.has_target_humidity) {
    append(ClimateTileSlotKind::TARGET_HUMIDITY);
  }
  if (state.hvac_mode[0]) {
    append(ClimateTileSlotKind::HVAC_MODE);
  }
  return count;
}

uint8_t build_slot_kinds(
    const Tile& tile, const ClimateState& state,
    ClimateTileSlotKind* out,
    ClimateTileTargetLayout* out_layouts,
    uint8_t capacity) {
  if (!out || capacity == 0) return 0;
  const uint8_t slot_capacity =
      std::min<uint8_t>(
          capacity, climateTileSlotCapacity(tile));
  ClimateTileSlotKind automatic[CLIMATE_TILE_MAX_CONTENT_SLOTS] = {};
  const uint8_t automatic_count =
      build_automatic_slot_kinds(
          tile, state, automatic, slot_capacity);

  bool explicitly_configured[
      static_cast<uint8_t>(ClimateTileSlotKind::HVAC_MODE) + 1] = {};
  for (uint8_t slot = 0; slot < slot_capacity; ++slot) {
    const ClimateTileContent configured =
        getClimateTileSlotContent(tile, slot);
    if (configured == CLIMATE_TILE_CONTENT_AUTO ||
        configured == CLIMATE_TILE_CONTENT_EMPTY) {
      continue;
    }
    const ClimateTileSlotKind kind = configured_slot_kind(configured);
    explicitly_configured[static_cast<uint8_t>(kind)] = true;
  }

  uint8_t count = 0;
  uint8_t used_units = 0;
  uint8_t automatic_cursor = 0;
  for (uint8_t slot = 0; slot < slot_capacity; ++slot) {
    const ClimateTileContent configured =
        getClimateTileSlotContent(tile, slot);
    ClimateTileSlotKind kind = ClimateTileSlotKind::NONE;
    ClimateTileTargetLayout layout =
        CLIMATE_TILE_TARGET_LAYOUT_AUTO;
    if (configured == CLIMATE_TILE_CONTENT_AUTO) {
      while (automatic_cursor < automatic_count) {
        const ClimateTileSlotKind candidate =
            automatic[automatic_cursor++];
        if (explicitly_configured[
                static_cast<uint8_t>(candidate)]) {
          continue;
        }
        kind = candidate;
        break;
      }
    } else if (configured != CLIMATE_TILE_CONTENT_EMPTY) {
      kind = configured_slot_kind(configured);
      layout = getClimateTileTargetLayout(tile, slot);
    }
    if (kind != ClimateTileSlotKind::NONE) {
      const uint8_t cost = slot_unit_cost(kind);
      if (used_units + cost <= slot_capacity) {
        out[count] = kind;
        if (out_layouts) out_layouts[count] = layout;
        ++count;
        used_units += cost;
      }
    }
  }
  return count;
}

void layout_climate_slots(
    ClimateTileWidgets& widgets, const Tile& tile) {
  const uint8_t count = widgets.active_slot_count;
  if (count == 0) return;

  const uint8_t span_w = std::max<uint8_t>(1, tile.span_w);
  const uint8_t span_h = std::max<uint8_t>(1, tile.span_h);
  const uint8_t columns = span_w >= 2 ? 2 : 1;
  const uint8_t capacity = climateTileSlotCapacity(tile);
  const uint8_t logical_rows =
      std::max<uint8_t>(1, (capacity + columns - 1) / columns);
  const lv_coord_t tile_w =
      static_cast<lv_coord_t>(
          span_w * GRID_CELL_W + (span_w - 1) * GRID_GAP);
  const lv_coord_t tile_h =
      static_cast<lv_coord_t>(
          span_h * GRID_CELL_H + (span_h - 1) * GRID_GAP);
  // Climate cards retain the original 20/24 px tile padding. Child
  // coordinates therefore address the padded content box, not the full card.
  // Adjustable controls use the otherwise unused outer 10 px on both sides.
  const lv_coord_t content_x = -10;
  const lv_coord_t content_w = tile_w - 20;
  const lv_coord_t column_gap = 10;
  const lv_coord_t slot_w =
      (content_w - static_cast<lv_coord_t>(columns - 1) * column_gap) /
      columns;
  const lv_coord_t slot_h = span_h == 1 ? 58 : 62;
  lv_coord_t first_row_y = 0;
  if (span_h == 1) {
    // Keep every value in a wide 2x1 tile on exactly the same vertical
    // value line as the unchanged 1x1 tile (LV_ALIGN_CENTER, y = 28).
    lv_obj_t* card = lv_obj_get_parent(widgets.slot_roots[0]);
    if (card) {
      lv_obj_update_layout(card);
      first_row_y =
          (lv_obj_get_content_height(card) - slot_h) / 2 + 28;
    }
  } else {
    // The first row of every taller climate tile must sit on the exact same
    // center line as the unchanged 1x1 value. Only the following rows extend
    // downwards into the additional tile height.
    const lv_coord_t one_by_one_content_h = GRID_CELL_H - 48;
    const lv_coord_t shared_value_center =
        one_by_one_content_h / 2 + 28;
    first_row_y = shared_value_center - slot_h / 2;
  }
  const lv_coord_t last_row_y =
      first_row_y +
      static_cast<lv_coord_t>(span_h - 1) *
          (GRID_CELL_H + GRID_GAP);
  auto row_y = [&](uint8_t row) -> lv_coord_t {
    if (logical_rows <= 1) return first_row_y;
    return first_row_y +
           static_cast<lv_coord_t>(
               (static_cast<int32_t>(last_row_y - first_row_y) * row +
                (logical_rows - 1) / 2) /
               (logical_rows - 1));
  };

  bool occupied[3][2] = {};
  for (uint8_t i = 0; i < ClimateTileWidgets::kMaxSlots; ++i) {
    lv_obj_t* root = widgets.slot_roots[i];
    if (!root) continue;
    if (i >= count) {
      lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
      continue;
    }

    const ClimateTileSlotKind kind =
        static_cast<ClimateTileSlotKind>(widgets.slot_kinds[i]);
    const bool adjustable = slot_is_adjustable(kind);
    uint8_t row = 0;
    uint8_t column = 0;
    uint8_t row_span = 1;
    uint8_t column_span = 1;
    bool placed = false;

    auto try_vertical_target = [&]() {
      for (uint8_t candidate_row = 0;
           candidate_row + 1 < logical_rows && !placed;
           ++candidate_row) {
        for (uint8_t candidate_column = 0;
             candidate_column < columns && !placed;
             ++candidate_column) {
          if (!occupied[candidate_row][candidate_column] &&
              !occupied[candidate_row + 1][candidate_column]) {
            row = candidate_row;
            column = candidate_column;
            row_span = 2;
            column_span = 1;
            placed = true;
          }
        }
      }
    };
    auto try_horizontal_target = [&]() {
      if (columns < 2) return;
      for (uint8_t candidate_row = 0;
           candidate_row < logical_rows && !placed;
           ++candidate_row) {
        if (!occupied[candidate_row][0] &&
            !occupied[candidate_row][1]) {
          row = candidate_row;
          column = 0;
          row_span = 1;
          column_span = 2;
          placed = true;
        }
      }
    };

    if (adjustable) {
      const ClimateTileTargetLayout requested =
          static_cast<ClimateTileTargetLayout>(
              widgets.slot_layouts[i]);
      if (requested == CLIMATE_TILE_TARGET_LAYOUT_HORIZONTAL) {
        try_horizontal_target();
        try_vertical_target();
      } else if (requested == CLIMATE_TILE_TARGET_LAYOUT_VERTICAL) {
        try_vertical_target();
        try_horizontal_target();
      } else if (columns == 1) {
        try_vertical_target();
      } else if (logical_rows == 1) {
        try_horizontal_target();
      } else {
        try_vertical_target();
        try_horizontal_target();
      }
    } else {
      for (uint8_t candidate_row = 0;
           candidate_row < logical_rows && !placed;
           ++candidate_row) {
        for (uint8_t candidate_column = 0;
             candidate_column < columns && !placed;
             ++candidate_column) {
          if (!occupied[candidate_row][candidate_column]) {
            row = candidate_row;
            column = candidate_column;
            placed = true;
          }
        }
      }
    }

    if (!placed) {
      lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    for (uint8_t occupied_row = row;
         occupied_row < row + row_span; ++occupied_row) {
      for (uint8_t occupied_column = column;
           occupied_column < column + column_span;
           ++occupied_column) {
        occupied[occupied_row][occupied_column] = true;
      }
    }

    const bool horizontal_target =
        adjustable && column_span == 2;
    const bool vertical_target =
        adjustable && row_span == 2;

    lv_coord_t root_x =
        content_x + column * (slot_w + column_gap);
    lv_coord_t root_y = row_y(row);
    lv_coord_t root_w = slot_w;
    lv_coord_t root_h = slot_h;
    if (horizontal_target) {
      root_x = content_x;
      root_w = content_w;
    } else if (vertical_target) {
      const lv_coord_t second_row_y =
          row_y(row + 1);
      root_h = second_row_y - root_y + slot_h;
    } else if (count == 1) {
      root_x = content_x;
      root_w = content_w;
    }

    lv_obj_clear_flag(root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(root, root_x, root_y);
    lv_obj_set_size(root, root_w, root_h);

    lv_obj_t* minus = lv_obj_get_child(root, 0);
    lv_obj_t* value = lv_obj_get_child(root, 1);
    lv_obj_t* plus = lv_obj_get_child(root, 2);
    lv_obj_t* caption = lv_obj_get_child(root, 3);

    if (horizontal_target) {
      const lv_coord_t caption_w = 108;
      const lv_coord_t button_w = 32;
      if (caption) {
        lv_obj_set_width(caption, caption_w - 12);
        lv_obj_set_style_text_align(
            caption, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(caption, LV_ALIGN_LEFT_MID, 12, 0);
      }
      if (minus) {
        lv_obj_set_size(minus, button_w, root_h);
        lv_obj_align(minus, LV_ALIGN_LEFT_MID, caption_w, 0);
      }
      if (plus) {
        lv_obj_set_size(plus, button_w, root_h);
        lv_obj_align(plus, LV_ALIGN_RIGHT_MID, 0, 0);
      }
      if (value) {
        lv_obj_set_width(
            value, root_w - caption_w - button_w * 2);
        lv_obj_set_style_text_font(value, FONT_VALUE, 0);
        lv_obj_align(
            value, LV_ALIGN_LEFT_MID,
            caption_w + button_w, 0);
      }
    } else if (vertical_target) {
      const lv_coord_t button_h = 40;
      if (caption) {
        lv_obj_set_width(caption, root_w - 16);
        lv_obj_set_style_text_align(
            caption, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(caption, LV_ALIGN_TOP_MID, 0, 10);
      }
      if (value) {
        lv_obj_set_width(value, root_w - 12);
        lv_obj_set_style_text_font(value, FONT_VALUE, 0);
        lv_obj_align(value, LV_ALIGN_CENTER, 0, -2);
      }
      if (minus) {
        lv_obj_set_size(minus, root_w / 2, button_h);
        lv_obj_align(minus, LV_ALIGN_BOTTOM_LEFT, 0, -2);
      }
      if (plus) {
        lv_obj_set_size(
            plus, root_w - root_w / 2, button_h);
        lv_obj_align(plus, LV_ALIGN_BOTTOM_RIGHT, 0, -2);
      }
    } else {
      if (caption) {
        lv_obj_add_flag(caption, LV_OBJ_FLAG_HIDDEN);
      }
      if (minus) {
        lv_obj_set_size(minus, 34, root_h);
        lv_obj_align(minus, LV_ALIGN_LEFT_MID, 0, 0);
      }
      if (plus) {
        lv_obj_set_size(plus, 34, root_h);
        lv_obj_align(plus, LV_ALIGN_RIGHT_MID, 0, 0);
      }
      if (value) {
        lv_obj_set_width(value, root_w);
        lv_obj_align(value, LV_ALIGN_CENTER, 0, 0);
      }
    }
  }
}

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
  init.icon_visible = !isMdiIconDisabled(tile->icon_name);
  init.dynamic_icon = init.icon_visible && !configured_icon.length();
  init.icon_name = climate_tile_base_icon(*tile);
  init.hvac_mode = state.hvac_mode;
  init.hvac_action = state.hvac_action;
  init.hvac_modes = climateHvacModesCsv(state.hvac_modes_mask);
  init.preset_mode = climatePresetName(state.preset_mode_id);
  init.preset_modes = climatePresetModesCsv(state.preset_modes_mask);
  init.fan_mode = state.fan_mode;
  init.fan_modes = climateFanModesCsv(state.fan_modes_mask);
  init.swing_mode = state.swing_mode;
  init.swing_modes = climateSwingModesCsv(state.swing_modes_mask);
  init.swing_horizontal_mode = state.swing_horizontal_mode;
  init.swing_horizontal_modes =
      climateHorizontalSwingModesCsv(state.swing_horizontal_modes_mask);
  init.temperature_unit = state.temperature_unit;
  init.current_temperature = state.current_temperature;
  init.current_humidity = state.current_humidity;
  init.target_temperature = state.target_temperature;
  init.target_humidity = state.target_humidity;
  init.target_temp_low = state.target_temp_low;
  init.target_temp_high = state.target_temp_high;
  init.min_temp = state.min_temp;
  init.max_temp = state.max_temp;
  init.min_humidity = state.min_humidity;
  init.max_humidity = state.max_humidity;
  init.target_temp_step = state.target_temp_step;
  init.has_current_temperature = state.has_current_temperature;
  init.has_current_humidity = state.has_current_humidity;
  init.has_target_temperature = state.has_target_temperature;
  init.has_target_humidity = state.has_target_humidity;
  init.has_target_range = state.has_target_range;
  return init;
}

void climate_adjust_event_cb(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) return;
  ClimateAdjustEventData* data =
      static_cast<ClimateAdjustEventData*>(
          lv_event_get_user_data(event));
  if (!data || data->index >= TILES_PER_GRID ||
      data->slot_index >= ClimateTileWidgets::kMaxSlots) {
    return;
  }
  ClimateTileWidgets* widgets =
      tile_renderer_get_climate_widgets(data->grid_type);
  ClimateState* states =
      tile_renderer_get_climate_states(data->grid_type);
  const Tile* tile =
      tile_renderer_get_tile_config(data->grid_type, data->index);
  if (!widgets || !states || !tile || !tile->sensor_entity.length()) {
    return;
  }

  ClimateState& state = states[data->index];
  const ClimateTileSlotKind kind =
      static_cast<ClimateTileSlotKind>(
          widgets[data->index].slot_kinds[data->slot_index]);
  const float direction = data->direction < 0 ? -1.0f : 1.0f;
  const float temperature_step =
      (state.target_temp_step > 0.0f &&
       state.target_temp_step <= 10.0f)
          ? state.target_temp_step
          : 0.5f;

  switch (kind) {
    case ClimateTileSlotKind::TARGET_TEMPERATURE:
      if (!state.has_target_temperature) return;
      state.target_temperature = clamp_value(
          snap_value(
              state.target_temperature +
                  direction * temperature_step,
              temperature_step),
          state.min_temp, state.max_temp);
      mqttPublishClimateTemperature(
          tile->sensor_entity.c_str(), state.target_temperature);
      break;
    case ClimateTileSlotKind::TARGET_TEMPERATURE_LOW:
      if (!state.has_target_range) return;
      state.target_temp_low = clamp_value(
          snap_value(
              state.target_temp_low +
                  direction * temperature_step,
              temperature_step),
          state.min_temp, state.target_temp_high);
      mqttPublishClimateTemperature(
          tile->sensor_entity.c_str(), 0.0f, true,
          state.target_temp_low, state.target_temp_high);
      break;
    case ClimateTileSlotKind::TARGET_TEMPERATURE_HIGH:
      if (!state.has_target_range) return;
      state.target_temp_high = clamp_value(
          snap_value(
              state.target_temp_high +
                  direction * temperature_step,
              temperature_step),
          state.target_temp_low, state.max_temp);
      mqttPublishClimateTemperature(
          tile->sensor_entity.c_str(), 0.0f, true,
          state.target_temp_low, state.target_temp_high);
      break;
    case ClimateTileSlotKind::TARGET_HUMIDITY:
      if (!state.has_target_humidity) return;
      state.target_humidity = clamp_value(
          roundf(state.target_humidity + direction),
          state.min_humidity, state.max_humidity);
      mqttPublishClimateHumidity(
          tile->sensor_entity.c_str(), state.target_humidity);
      break;
    default:
      return;
  }

  refresh_climate_tile_content(
      data->grid_type, data->index, state);
  const ClimateEventData popup_data{data->grid_type, data->index};
  update_climate_popup(popup_init_for(&popup_data));
}

void climate_adjust_event_data_delete_cb(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_DELETE) return;
  delete static_cast<ClimateAdjustEventData*>(
      lv_event_get_user_data(event));
}

lv_obj_t* create_climate_slot(
    lv_obj_t* card, GridType grid_type, uint8_t index,
    uint8_t slot_index) {
  lv_obj_t* root = lv_obj_create(card);
  lv_obj_set_style_bg_color(
      root, lv_color_hex(0x3A3A3A), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(root, 0, 0);
  lv_obj_set_style_border_color(
      root, lv_color_hex(0x4A4A4A), LV_PART_MAIN);
  lv_obj_set_style_border_opa(root, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(root, 0, 0);
  lv_obj_set_style_radius(root, 12, 0);
  lv_obj_set_style_pad_all(root, 0, 0);
  lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_CLICKABLE);

  auto create_adjust_button = [&](int8_t direction) {
    lv_obj_t* button = lv_button_create(root);
    lv_obj_set_style_bg_opa(
        button, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        button, lv_color_hex(0x5A5A5A),
        LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(
        button, LV_OPA_50,
        LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_remove_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    disable_pressed_button_animation(button);

    lv_obj_t* label = lv_label_create(button);
    set_label_style(label, lv_color_white(), &ui_font_24);
    lv_label_set_text(label, direction < 0 ? "-" : "+");
    lv_obj_center(label);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);

    ClimateAdjustEventData* data =
        new ClimateAdjustEventData{
            grid_type, index, slot_index, direction};
    lv_obj_add_event_cb(
        button, climate_adjust_event_cb,
        LV_EVENT_SHORT_CLICKED, data);
    lv_obj_add_event_cb(
        button, climate_adjust_event_data_delete_cb,
        LV_EVENT_DELETE, data);
    return button;
  };

  create_adjust_button(-1);

  lv_obj_t* value = lv_label_create(root);
  set_label_style(value, lv_color_white(), &ui_font_24);
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(value, LV_LABEL_LONG_CLIP);
  lv_label_set_text(value, "--");
  lv_obj_clear_flag(value, LV_OBJ_FLAG_CLICKABLE);

  create_adjust_button(1);

  lv_obj_t* caption = lv_label_create(root);
  set_label_style(caption, lv_color_hex(0xE0E0E0), FONT_TITLE);
  lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(caption, LV_LABEL_LONG_DOT);
  lv_label_set_text(caption, "");
  lv_obj_clear_flag(caption, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(caption, LV_OBJ_FLAG_HIDDEN);
  return root;
}

}  // namespace

void refresh_climate_tile_content(
    GridType grid_type, uint8_t index,
    const ClimateState& state) {
  if (index >= TILES_PER_GRID) return;
  ClimateTileWidgets* widgets =
      tile_renderer_get_climate_widgets(grid_type);
  const Tile* tile =
      tile_renderer_get_tile_config(grid_type, index);
  if (!widgets || !tile) return;

  ClimateTileWidgets& widget = widgets[index];
  const bool has_slot_layout = widget.slot_roots[0] != nullptr;
  ClimateTileSlotKind kinds[ClimateTileWidgets::kMaxSlots] = {};
  ClimateTileTargetLayout layouts[
      ClimateTileWidgets::kMaxSlots] = {};
  const uint8_t count =
      build_slot_kinds(
          *tile, state, kinds, layouts,
          ClimateTileWidgets::kMaxSlots);

  if (!has_slot_layout) {
    widget.active_slot_count = 0;
    if (widget.value_label) {
      const ClimateTileSlotKind kind =
          count > 0 ? kinds[0]
                    : ClimateTileSlotKind::CURRENT_TEMPERATURE;
      const String text = climate_slot_text(kind, state);
      lv_label_set_text(widget.value_label, text.c_str());
      lv_obj_clear_flag(widget.value_label, LV_OBJ_FLAG_HIDDEN);
    }
    return;
  }

  if (widget.value_label) {
    if (!state.valid) {
      lv_obj_clear_flag(widget.value_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(widget.value_label, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (!state.valid && has_slot_layout) {
    widget.active_slot_count = 0;
    for (uint8_t i = 0;
         i < ClimateTileWidgets::kMaxSlots; ++i) {
      if (widget.slot_roots[i]) {
        lv_obj_add_flag(
            widget.slot_roots[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    return;
  }

  widget.active_slot_count = count;

  for (uint8_t i = 0; i < ClimateTileWidgets::kMaxSlots; ++i) {
    lv_obj_t* root = widget.slot_roots[i];
    if (!root) continue;
    if (i >= count) {
      widget.slot_kinds[i] =
          static_cast<uint8_t>(ClimateTileSlotKind::NONE);
      widget.slot_layouts[i] =
          static_cast<uint8_t>(CLIMATE_TILE_TARGET_LAYOUT_AUTO);
      lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
      continue;
    }

    const ClimateTileSlotKind kind = kinds[i];
    widget.slot_kinds[i] = static_cast<uint8_t>(kind);
    widget.slot_layouts[i] = static_cast<uint8_t>(layouts[i]);
    const bool adjustable = slot_is_adjustable(kind);
    lv_obj_set_style_bg_opa(
        root, LV_OPA_TRANSP,
        LV_PART_MAIN);
    lv_obj_set_style_border_width(
        root, adjustable ? 1 : 0, LV_PART_MAIN);

    lv_obj_t* minus = lv_obj_get_child(root, 0);
    lv_obj_t* value = lv_obj_get_child(root, 1);
    lv_obj_t* plus = lv_obj_get_child(root, 2);
    lv_obj_t* caption = lv_obj_get_child(root, 3);
    if (minus) {
      if (adjustable) {
        lv_obj_clear_flag(minus, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(minus, LV_OBJ_FLAG_HIDDEN);
      }
    }
    if (plus) {
      if (adjustable) {
        lv_obj_clear_flag(plus, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(plus, LV_OBJ_FLAG_HIDDEN);
      }
    }
    if (value) {
      const String text = climate_slot_text(kind, state);
      lv_label_set_text(value, text.c_str());
      lv_obj_set_style_text_color(value, lv_color_white(), 0);
      lv_obj_set_style_text_font(
          value,
          adjustable
              ? FONT_VALUE
              : (kind == ClimateTileSlotKind::HVAC_MODE
                     ? FONT_TITLE
                     : FONT_VALUE),
          0);
    }
    if (caption) {
      if (adjustable) {
        const String text = climate_slot_caption(kind, state);
        lv_label_set_text(caption, text.c_str());
        lv_obj_clear_flag(caption, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(caption, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }

  layout_climate_slots(widget, *tile);
}

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
  const bool legacy_one_by_one =
      tile.span_w == 1 && tile.span_h == 1;
  lv_obj_set_style_pad_hor(card, 20, 0);
  lv_obj_set_style_pad_ver(card, 24, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  disable_pressed_button_animation(card);
  set_tile_grid_cell(card, col, row, tile.span_w, tile.span_h);

  const bool icon_disabled = isMdiIconDisabled(tile.icon_name);
  const String configured_icon = normalizeMdiIconName(tile.icon_name);
  const bool dynamic_icon = !icon_disabled && !configured_icon.length();
  String icon_name = climate_tile_base_icon(tile);

  lv_obj_t* icon_label = nullptr;
  String icon_char = getMdiChar(icon_name);
  if (!icon_char.length()) icon_char = getMdiChar("thermostat");
  if (!icon_disabled && FONT_MDI_ICONS) {
    icon_label = lv_label_create(card);
    set_label_style(icon_label, lv_color_white(), FONT_MDI_ICONS);
    lv_label_set_text(icon_label, icon_char.c_str());
    lv_obj_align(
        icon_label, LV_ALIGN_TOP_LEFT,
        -8, -8);
  }

  if (tile.title.length()) {
    lv_obj_t* title = lv_label_create(card);
    set_label_style(title, lv_color_white(), FONT_TITLE);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, LV_PCT(70));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(title, tile.title.c_str());
    lv_obj_align(
        title, LV_ALIGN_TOP_RIGHT,
        4, 4);
  }

  ClimateTileWidgets* widgets = tile_renderer_get_climate_widgets(grid_type);
  if (widgets && index < TILES_PER_GRID) {
    ClimateTileWidgets& widget = widgets[index];
    widget.icon_label = icon_label;
    widget.dynamic_icon = dynamic_icon;
    // A newly created card must accept the next state payload even when the
    // same entity/value was rendered by a previous cached card instance.
    widget.last_payload_hash = 0;
    lv_obj_t* value = lv_label_create(card);
    set_label_style(value, lv_color_white(), FONT_VALUE);
    lv_obj_set_width(value, LV_PCT(100));
    lv_obj_set_style_text_align(
        value, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(
        value, "-- \xC2\xB0"
               "C");
    lv_obj_align(value, LV_ALIGN_CENTER, 0, 28);
    widget.value_label = value;

    if (!legacy_one_by_one) {
      for (uint8_t slot = 0;
           slot < ClimateTileWidgets::kMaxSlots; ++slot) {
        widget.slot_roots[slot] =
            create_climate_slot(
                card, grid_type, index, slot);
      }
    }
    ClimateState* states =
        tile_renderer_get_climate_states(grid_type);
    const ClimateState initial_state =
        states ? states[index] : ClimateState{};
    refresh_climate_tile_content(
        grid_type, index, initial_state);
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

#pragma once

#include <Arduino.h>
#include <lvgl.h>

struct ClimatePopupInit {
  String entity_id;
  String title;
  String icon_name;
  String hvac_mode;
  String hvac_action;
  String hvac_modes;
  String preset_mode;
  String preset_modes;
  String fan_mode;
  String fan_modes;
  String swing_mode;
  String swing_modes;
  String swing_horizontal_mode;
  String swing_horizontal_modes;
  String temperature_unit;
  bool icon_visible = true;
  bool dynamic_icon = true;
  float current_temperature = 0.0f;
  float current_humidity = 0.0f;
  float target_temperature = 20.0f;
  float target_humidity = 50.0f;
  float target_temp_low = 18.0f;
  float target_temp_high = 24.0f;
  float min_temp = 7.0f;
  float max_temp = 35.0f;
  float min_humidity = 30.0f;
  float max_humidity = 99.0f;
  float target_temp_step = 0.5f;
  bool has_current_temperature = false;
  bool has_current_humidity = false;
  bool has_target_temperature = false;
  bool has_target_humidity = false;
  bool has_target_range = false;
};

void show_climate_popup(const ClimatePopupInit& init);
void update_climate_popup(const ClimatePopupInit& init);
void preload_climate_popup();
void hide_climate_popup();

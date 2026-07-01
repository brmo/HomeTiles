#include "src/types/pixelanim/web_handler.h"

void apply_pixelanim_fields_from_request(WebServer& server, Tile& tile) {
  // The chosen .panim file name lives in scene_alias (a generic, side-effect
  // free string slot, same approach the text tile uses for its value).
  tile.scene_alias = server.hasArg("animation_file") ? server.arg("animation_file") : "";
  // Playback speed (frames/sec) is stored in image_slideshow_sec for this type.
  int fps = server.hasArg("animation_fps") ? server.arg("animation_fps").toInt() : 10;
  if (fps < 1) fps = 1;
  if (fps > 30) fps = 30;
  tile.image_slideshow_sec = static_cast<uint16_t>(fps);
  tile.sensor_entity = "";
  tile.sensor_unit = "";
  tile.key_macro = "";
  tile.image_path = "";
  tile.sensor_decimals = 0xFF;
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
  tile.key_code = 0;
  tile.key_modifier = 0;
}

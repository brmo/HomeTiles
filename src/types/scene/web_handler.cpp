#include "src/types/scene/web_handler.h"

void apply_scene_fields_from_request(WebServer& server, Tile& tile) {
  tile.scene_alias = server.hasArg("scene_alias") ? server.arg("scene_alias") : "";
  tile.image_path = server.hasArg("image_path") ? server.arg("image_path") : "";
  tile.image_path.trim();
  Serial.printf("[Scene] apply_from_request: scene_alias='%s' image_path='%s'\n",
    tile.scene_alias.c_str(), tile.image_path.c_str());
  tile.sensor_decimals = 0xFF;
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
}

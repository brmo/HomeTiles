#include "src/types/template/web_handler.h"

void apply_template_fields_from_request(WebServer& server, Tile& tile) {
  tile.scene_alias = server.hasArg("template_value") ? server.arg("template_value") : "";
  tile.sensor_decimals = 0xFF;
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
}

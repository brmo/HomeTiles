#include "src/types/sensor/web_handler.h"

void apply_sensor_fields_from_request(WebServer& server, Tile& tile) {
  tile.sensor_entity = server.hasArg("sensor_entity") ? server.arg("sensor_entity") : "";
  tile.sensor_unit = server.hasArg("sensor_unit") ? server.arg("sensor_unit") : "";

  uint8_t decimals = 0xFF;
  if (server.hasArg("sensor_decimals")) {
    String decStr = server.arg("sensor_decimals");
    decStr.trim();
    if (decStr.length() > 0) {
      int dec = decStr.toInt();
      if (dec < 0) dec = 0;
      if (dec > 6) dec = 6;
      decimals = static_cast<uint8_t>(dec);
    }
  }
  tile.sensor_decimals = decimals;
  uint8_t value_font = 0;
  if (server.hasArg("sensor_value_font")) {
    int raw = server.arg("sensor_value_font").toInt();
    value_font = (raw == 1 || raw == 2) ? static_cast<uint8_t>(raw) : 0;
  }
  tile.sensor_value_font = value_font;
  tile.sensor_gauge_enabled = false;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
  if (server.hasArg("sensor_gauge")) {
    tile.sensor_gauge_enabled = (server.arg("sensor_gauge").toInt() == 1);
  }
  if (server.hasArg("sensor_gauge_min")) {
    String raw = server.arg("sensor_gauge_min");
    raw.trim();
    if (raw.length() > 0) tile.sensor_gauge_min = raw.toInt();
  }
  if (server.hasArg("sensor_gauge_max")) {
    String raw = server.arg("sensor_gauge_max");
    raw.trim();
    if (raw.length() > 0) tile.sensor_gauge_max = raw.toInt();
  }
  if (tile.sensor_gauge_max <= tile.sensor_gauge_min) {
    tile.sensor_gauge_min = 0;
    tile.sensor_gauge_max = 100;
  }
}

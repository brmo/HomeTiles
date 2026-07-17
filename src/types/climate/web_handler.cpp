#include "src/types/climate/web_handler.h"

void apply_climate_fields_from_request(WebServer& server, Tile& tile) {
  tile.sensor_entity =
      server.hasArg("climate_entity") ? server.arg("climate_entity") : "";
  uint8_t popup_mode = TILE_POPUP_OPEN_SHORT_PRESS;
  if (server.hasArg("popup_open_mode")) {
    popup_mode =
        server.arg("popup_open_mode").toInt() == TILE_POPUP_OPEN_LONG_PRESS
            ? TILE_POPUP_OPEN_LONG_PRESS
            : TILE_POPUP_OPEN_SHORT_PRESS;
  }
  setTilePopupOpenMode(tile, popup_mode);
  tile.sensor_unit = "";
  tile.sensor_decimals = 1;
  tile.sensor_value_font = 0;
  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
}

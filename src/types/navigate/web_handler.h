#pragma once

#include <WebServer.h>
#include "src/tiles/tile_config.h"

bool apply_navigate_fields_from_request(
    WebServer& server,
    Tile& tile,
    uint16_t folder_id,
    TileConfig& tileConfig,
    String& error_message);

#pragma once

#include "src/tiles/tile_renderer.h"

lv_obj_t* render_climate_tile(lv_obj_t* parent,
                              int col,
                              int row,
                              const Tile& tile,
                              uint8_t index,
                              GridType grid_type);

void refresh_climate_tile_content(GridType grid_type,
                                  uint8_t index,
                                  const ClimateState& state);

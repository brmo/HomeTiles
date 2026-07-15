#pragma once

#include <lvgl.h>

namespace ui_surface_style {

void apply_tile_border(lv_obj_t* obj, bool enabled);
void apply_global_tile_border(lv_obj_t* obj);

// Darf aus dem Web-Handler aufgerufen werden: setzt nur ein Flag. Die
// eigentliche LVGL-Aktualisierung erfolgt spaeter im sicheren UI-Durchlauf.
void request_global_tile_border_refresh();
void process_pending_updates();

}  // namespace ui_surface_style

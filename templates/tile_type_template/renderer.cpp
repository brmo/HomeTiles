#include "src/types/template/renderer.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/mdi_icons.h"
#include <Arduino.h>

lv_obj_t* render_TEMPLATE_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index) {
  (void)index;

  lv_obj_t* card = lv_button_create(parent);
  if (!card) return nullptr;

  uint32_t card_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(card, lv_color_hex(card_color), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  set_tile_grid_cell(card, col, row, tile.span_w, tile.span_h);

  // Optional: icon/title like other tiles.
  // Use tile.<field> to render your content.

  return card;
}

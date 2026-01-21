#include "src/types/image/renderer.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/mdi_icons.h"
#include "src/ui/image_popup.h"
#include <Arduino.h>

struct ImageEventData {
  String image_path;
  uint16_t slideshow_sec;
};

lv_obj_t* render_image_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index) {
  Serial.printf("[TileRenderer] render_image_tile: title='%s', image_path='%s'\n", tile.title.c_str(), tile.image_path.c_str());

  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 22, 0);
  lv_obj_set_style_border_width(btn, 0, 0);

  // Farbe verwenden (Standard: 0x353535 wenn color = 0)
  uint32_t btn_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_DEFAULT);

  // Pressed-State: 10% heller
  uint32_t pressed_color = btn_color + 0x101010;
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

  set_tile_grid_cell(btn, col, row, tile.span_w, tile.span_h);

  // Icon Label (optional)
  lv_obj_t* icon_lbl = nullptr;
  bool has_icon = tile.icon_name.length() > 0;
  bool has_title = tile.title.length() > 0;

  if (has_icon && FONT_MDI_ICONS != nullptr) {
    String iconChar = getMdiChar(tile.icon_name);
    if (iconChar.length() > 0) {
      icon_lbl = lv_label_create(btn);
      if (icon_lbl) {
        set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
        lv_label_set_text(icon_lbl, iconChar.c_str());

        if (has_title) {
          lv_obj_align(icon_lbl, LV_ALIGN_CENTER, 0, -20);  // Icon oben
        } else {
          lv_obj_center(icon_lbl);  // Icon mittig
        }
      }
    }
  }

  // Title Label
  if (has_title) {
    lv_obj_t* l = lv_label_create(btn);
    if (l) {
      set_label_style(l, lv_color_white(), FONT_TITLE);
      lv_label_set_text(l, tile.title.c_str());

      if (icon_lbl) {
        lv_obj_align(l, LV_ALIGN_CENTER, 0, 35);  // Title unten
      } else {
        lv_obj_center(l);  // Title mittig
      }
    }
  }

  // Event-Handler für Image-Popup
  if (tile.image_path.length() > 0) {
    Serial.printf("[TileRenderer] Registriere Click-Event für image_path='%s'\n", tile.image_path.c_str());

    // Allocate permanent storage for event data
    ImageEventData* event_data = new ImageEventData{tile.image_path, tile.image_slideshow_sec};

    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
          ImageEventData* data = static_cast<ImageEventData*>(lv_event_get_user_data(e));
          if (data && data->image_path.length() > 0) {
            Serial.printf("[Tile] Öffne Bild: %s\n", data->image_path.c_str());
            show_image_popup(data->image_path.c_str(), data->slideshow_sec);
          } else {
            Serial.println("[Tile] FEHLER: Keine event_data oder image_path leer!");
          }
        },
        LV_EVENT_CLICKED,
        event_data);

    // Cleanup on delete
    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
          ImageEventData* data = static_cast<ImageEventData*>(lv_event_get_user_data(e));
          delete data;
        },
        LV_EVENT_DELETE,
        event_data);
  } else {
    Serial.println("[TileRenderer] WARNUNG: image_path ist leer - kein Click-Event registriert!");
  }

  return btn;
}



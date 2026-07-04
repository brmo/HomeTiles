#include "src/ui/ui_keyboard.h"

#include <cstring>

namespace {

constexpr uint32_t kKeyBg = 0x3A3A3A;
constexpr uint32_t kKeyBgCtrl = 0x2C2C2C;      // Steuertasten (ABC, 1#, Pfeile, ...)
constexpr uint32_t kKeyBgPressed = 0x5A5A5A;
constexpr uint32_t kKeyBgOk = 0x2E7D32;
constexpr uint32_t kKeyBgOkPressed = 0x43A047;
constexpr uint32_t kKeyText = 0xEDEDED;

// Buttonmatrix-Items kennen keine Einzel-Styles, und LVGLs Default-Theme legt
// auf die gerade fokussierte Taste (waehrend des Tippens staendig wechselnd)
// einen eigenen Outline-Ring in Akzentfarbe - dessen State-Selektor ist
// spezifischer als unser einfacher (State-loser) lokaler Style und gewinnt
// daher trotz lokalem Style (LVGL waehlt den spezifischsten State-Treffer,
// nicht "lokal schlaegt immer Theme"). Das erzeugte den gemeldeten Gruenstich.
// Deshalb werden ALLE Tastenfarben UND jeglicher Rahmen/Outline beim Zeichnen
// per Draw-Task-Event erzwungen: Buchstaben dunkelgrau, Steuertasten dunkler,
// OK-Taste gruen, Text weiss, kein Rahmen/Outline auf irgendeiner Taste.
void kb_draw_task_cb(lv_event_t* e) {
  lv_draw_task_t* task = lv_event_get_draw_task(e);
  if (!task) return;
  lv_draw_dsc_base_t* base =
      static_cast<lv_draw_dsc_base_t*>(lv_draw_task_get_draw_dsc(task));
  if (!base || base->part != LV_PART_ITEMS) return;

  lv_draw_label_dsc_t* label = lv_draw_task_get_label_dsc(task);
  if (label) {
    label->color = lv_color_hex(kKeyText);
    return;
  }

  // Border UND Outline laufen beide als LV_DRAW_TASK_TYPE_BORDER; beide
  // komplett kappen, unabhaengig davon welcher Theme-Zustand sie ausgeloest hat.
  lv_draw_border_dsc_t* border = lv_draw_task_get_border_dsc(task);
  if (border) {
    border->opa = LV_OPA_TRANSP;
    return;
  }

  lv_draw_fill_dsc_t* fill = lv_draw_task_get_fill_dsc(task);
  if (!fill) return;

  lv_obj_t* kb = static_cast<lv_obj_t*>(lv_event_get_target(e));
  const uint32_t id = base->id1;
  const char* txt = lv_buttonmatrix_get_button_text(kb, id);
  const bool is_ok = txt && strcmp(txt, LV_SYMBOL_OK) == 0;
  const bool is_ctrl =
      lv_buttonmatrix_has_button_ctrl(kb, id, LV_BUTTONMATRIX_CTRL_CHECKED);
  const bool pressed = lv_obj_has_state(kb, LV_STATE_PRESSED) &&
                       lv_buttonmatrix_get_selected_button(kb) == id;

  uint32_t color;
  if (is_ok) {
    color = pressed ? kKeyBgOkPressed : kKeyBgOk;
  } else if (pressed) {
    color = kKeyBgPressed;
  } else if (is_ctrl) {
    color = kKeyBgCtrl;
  } else {
    color = kKeyBg;
  }
  fill->color = lv_color_hex(color);
  fill->opa = LV_OPA_COVER;
  fill->grad.dir = LV_GRAD_DIR_NONE;
}

}  // namespace

lv_obj_t* ui_keyboard_create(lv_obj_t* parent) {
  lv_obj_t* kb = lv_keyboard_create(parent);

  // Rahmenlos und transparent: der Karten-Hintergrund scheint zwischen den
  // Tasten durch, die Tasten reichen bis an den Kartenrand.
  lv_obj_set_style_bg_opa(kb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(kb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(kb, 0, 0);
  lv_obj_set_style_pad_gap(kb, 6, 0);
  lv_obj_set_style_radius(kb, 0, 0);

  const bool large = lv_display_get_horizontal_resolution(nullptr) >= 1024;
  lv_obj_set_style_text_font(
      kb, large ? &lv_font_montserrat_24 : &lv_font_montserrat_20, LV_PART_ITEMS);

  lv_obj_set_style_bg_color(kb, lv_color_hex(kKeyBg), LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_text_color(kb, lv_color_hex(kKeyText), LV_PART_ITEMS);
  lv_obj_set_style_radius(kb, 10, LV_PART_ITEMS);
  lv_obj_set_style_border_opa(kb, LV_OPA_TRANSP, LV_PART_ITEMS);
  lv_obj_set_style_outline_opa(kb, LV_OPA_TRANSP, LV_PART_ITEMS);
  lv_obj_set_style_shadow_opa(kb, LV_OPA_TRANSP, LV_PART_ITEMS);
  // Steuertasten tragen im Keyboard-Map LV_BUTTONMATRIX_CTRL_CHECKED.
  lv_obj_set_style_bg_color(kb, lv_color_hex(kKeyBgCtrl),
                            LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(kb, lv_color_hex(kKeyBgPressed),
                            LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(kb, lv_color_hex(kKeyBgPressed),
                            LV_PART_ITEMS | LV_STATE_CHECKED | LV_STATE_PRESSED);

  lv_obj_add_flag(kb, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
  lv_obj_add_event_cb(kb, kb_draw_task_cb, LV_EVENT_DRAW_TASK_ADDED, nullptr);
  return kb;
}

void ui_keyboard_set_target(lv_obj_t* kb, lv_obj_t* ta, lv_obj_t* prev) {
  if (!kb || !ta) return;
  lv_keyboard_set_textarea(kb, ta);
  // Fokus-Zustand manuell setzen: nur so zeigt die Textarea ihren Cursor,
  // auch wenn der Nutzer direkt auf der Tastatur tippt statt ins Feld.
  if (prev && prev != ta) lv_obj_remove_state(prev, LV_STATE_FOCUSED);
  lv_obj_add_state(ta, LV_STATE_FOCUSED);
}

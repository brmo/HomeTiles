#pragma once

#include <lvgl.h>

// Dunkle Bildschirmtastatur im App-Look: dunkelgraue Tasten, abgesetzte
// Steuertasten, gruene OK-Taste. Wiederverwendbar fuer alle Settings-Seiten
// mit Texteingabe (WLAN, MQTT, ...).
lv_obj_t* ui_keyboard_create(lv_obj_t* parent);

// Setzt das Ziel-Textfeld der Tastatur und verschiebt den Fokus (Cursor +
// Rahmen-Hervorhebung) dorthin. prev darf nullptr sein.
void ui_keyboard_set_target(lv_obj_t* kb, lv_obj_t* ta, lv_obj_t* prev);

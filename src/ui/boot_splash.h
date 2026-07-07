#ifndef BOOT_SPLASH_H
#define BOOT_SPLASH_H

#include <stdint.h>

// Vollbild-Ladeanzeige waehrend setup() (Icon+HomeTiles+Version+Geraet wie im
// System-Popup, Fortschrittsbalken statt Buttons, keine Karte). Muss erst
// laufen, nachdem displayManager.init() erfolgreich war -- vorher gibt es
// noch keinen aktiven LVGL-Screen zum Zeichnen.
namespace BootSplash {

void show();

// percent: 0-100 (wird intern geklemmt). status_text darf nullptr sein,
// dann bleibt die Statuszeile leer/unveraendert. Aktualisiert nur die
// LVGL-Objekte -- der Aufrufer muss danach selbst lv_timer_handler() (und
// bei Bedarf den geraetespezifischen Refresh) pumpen, damit es auch auf dem
// Panel ankommt (siehe setup() in HomeTiles.ino).
void setProgress(uint8_t percent, const char* status_text);

// Entfernt das Overlay wieder vollstaendig (loescht die LVGL-Objekte).
void hide();

}  // namespace BootSplash

#endif  // BOOT_SPLASH_H

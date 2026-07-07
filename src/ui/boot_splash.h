#ifndef BOOT_SPLASH_H
#define BOOT_SPLASH_H

// Kurzer Vollbild-Begruessungsscreen waehrend setup() (Icon+HomeTiles+
// Version+Geraet wie im System-Popup, keine Karte/Buttons). Muss erst
// laufen, nachdem displayManager.init() erfolgreich war -- vorher gibt es
// noch keinen aktiven LVGL-Screen zum Zeichnen. Zeigt keinen echten
// Boot-Fortschritt mehr, nur kurz genug stehen, dass man Version/Geraet
// lesen kann, bevor auf die eigentliche Oberflaeche gewechselt wird
// (Mindestanzeigedauer siehe hide() in HomeTiles.ino).
namespace BootSplash {

void show();

// Muss einmal aufgerufen werden, nachdem waehrend des Boots neue
// Geschwister-Objekte auf dem aktiven Screen erzeugt wurden (z.B. nach dem
// UI-Build) -- sonst landen die neuen Tabs vor dem Splash, weil LVGL Kinder
// in Erzeugungsreihenfolge zeichnet.
void bringToFront();

// Entfernt das Overlay wieder vollstaendig (loescht die LVGL-Objekte).
void hide();

}  // namespace BootSplash

#endif  // BOOT_SPLASH_H

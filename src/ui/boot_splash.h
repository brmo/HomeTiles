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

// Baut nur die LVGL-Objekte auf. Das Panel ist nach displayManager.init()
// noch nicht sichtbar (kein displayWake() passiert) -- der Aufrufer muss
// direkt danach selbst BoardHAL::displayWake() + einen Refresh anstossen,
// siehe HomeTiles.ino (nutzt dort die geraetespezifischen Wake-Helfer, die
// nicht in dieses Modul exportiert sind).
void show();

// Entfernt das Overlay wieder vollstaendig (loescht die LVGL-Objekte). Erst
// aufrufen, wenn der Splash schon lang genug sichtbar war -- danach beginnt
// der Aufrufer mit dem eigentlichen UI-Aufbau auf demselben Screen, splash
// und Kacheln sollten sich also nie ueberlappen.
void hide();

}  // namespace BootSplash

#endif  // BOOT_SPLASH_H

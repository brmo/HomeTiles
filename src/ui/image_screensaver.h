#pragma once

#include <Arduino.h>

// Bild-Overlay im Popup-Format (quadratische Karte wie die anderen Popups,
// Radius 22) mit Uhrzeit/Datum darueber. Wird vom Clock-Tile per Tap
// geoeffnet; Bildquelle ist eine JPEG-Datei im SD-Ordner /wallpapers
// (Upload ueber den File-Manager im Web-Admin). Tap schliesst das Overlay.
struct ImageScreensaverInit {
  String file_name;     // Dateiname (basename) in /wallpapers
  uint8_t time_format;  // clock_tile::TIME_FORMAT_* (bereits aufgeloest)
  uint8_t date_format;  // clock_tile::DATE_FORMAT_* (bereits aufgeloest)
  bool show_time = true;
  bool show_date = true;
};

void show_image_screensaver(const ImageScreensaverInit& init);
void hide_image_screensaver();

// Dekodiert das Bild ein paar Sekunden spaeter im Hintergrund in den Cache,
// damit schon der erste Tap ohne SD-/Decode-Wartezeit oeffnet. Vom
// Clock-Renderer beim Kachel-Aufbau aufgerufen.
void preload_image_screensaver(const String& file_name);

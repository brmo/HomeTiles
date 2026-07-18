#include "src/core/i18n.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace i18n {

static const Strings kStringsDe = {
    "de",
    "de",
    "Sprache:",
    "Zeitzone:",
    "Zeitformat:",
    "Datumsformat:",
    "Auto (Sprache)",
    "Auto (Lokalisierung)",
    "24 Stunden",
    "12 Stunden",
    "English",
    "Deutsch",

    "Home",
    "Ordner ",

    "Waveshare Admin",
    "Waveshare Admin-Panel",
    "Konfiguration & Übersicht",
    "Klicke auf eine Kachel, um sie zu bearbeiten. Kacheln lassen sich per Drag & Drop verschieben und am Eckgriff vergrößern oder verkleinern. Wähle den Typ (Sensor/Wetter/Szene/Key/Ordner/Settings/Switch/Media/Bild/Uhr/Text) und passe die Einstellungen an.",
    "Ordner / Tab löschen",
    "Kachel Einstellungen",
    "Typ",
    "Typ gesperrt: Der Ordner enthält noch Kacheln. Erst leeren – Löschen der Kachel bleibt möglich.",
    "Titel",
    "Kachel-Titel",
    "Icon (MDI)",
    "z.B. home, thermometer, lightbulb",
    "Icon-Liste anzeigen",
    "Farbe",
    "Spalte",
    "Zeile",
    "Breite",
    "Höhe",
    "Änderungen werden automatisch gespeichert.",
    "Kopieren",
    "Einfügen",
    "Löschen",
    "Import / Export (alle Ordner & Kacheln)",
    "Export",
    "Import",
    "Import überschreibt die enthaltenen Ordner/Kacheln und, falls vorhanden, den Screensaver.",
    "WiFi",
    "MQTT",
    "Lokalisierung",
    "Screenshot & Diagnose",
    "Firmware Update",
    "Screenshot erstellen & herunterladen",
    "Speichert /ui_screenshot.jpg auf der microSD-Karte. Die vorhandene Datei wird überschrieben.",
    "Firmware-Datei",
    "Aktuelle Firmware",
    "Update",
    "Hier nur die update.bin hochladen. Die factory.bin ist nur für den ersten Flash gedacht.",
    "Datei auswählen",
    "Keine Datei ausgewählt",

    "WiFi Status",
    "Verbunden",
    "Getrennt",
    "Offline",
    "AP aktiv",
    "WLAN",
    "SSID",
    "IP",
    "Passwort",
    "Statische IP",
    "Gateway",
    "Subnetzmaske",
    "DNS-Server",
    "Leer lassen für DHCP",
    "MQTT nicht konfiguriert",
    "AP aktivieren",
    "AP beenden",
    "Ja",
    "Nein",

    "MQTT Host / IP",
    "Port",
    "Benutzername",
    "Passwort",
    "MQTT Client ID",
    "leer = automatisch",
    "Leer lassen = automatisch aus der MAC-Adresse erzeugen.",
    "Geräte-Topic Basis",
    "Home Assistant Prefix",
    "Uhrzeit Schriftgröße",
    "Datum Schriftgröße",
    "Speichern",
    "Gerät wirklich neu starten?",
    "Neustart",
    "MQTT-Konfiguration gespeichert",
    "Das Gerät verbindet sich neu ...",
    "Bridge-Konfiguration gespeichert",
    "Die Daten wurden per MQTT übertragen.",
    "Speichern fehlgeschlagen",

    "Waveshare WiFi-Konfiguration",
    "WiFi-Konfiguration",
    "Schritt 1: Mit WLAN verbinden",
    "WiFi-Verbindung",
    "SSID (Netzwerkname)",
    "Mein WiFi",
    "Passwort",
    "Passwort",
    "Leer lassen für offenes Netzwerk",
    "Hinweis:",
    "Nach erfolgreicher WLAN-Verbindung kannst du über das Webinterface im normalen Netzwerk die MQTT-Einstellungen konfigurieren.",
    "Speichern & Verbinden",
    "Erfolgreich gespeichert!",
    "Die Konfiguration wurde erfolgreich gespeichert.<br>Das Gerät wird jetzt neu gestartet und versucht sich mit dem WiFi zu verbinden.",
    "Du wirst in 10 Sekunden automatisch weitergeleitet.<br>Falls die Verbindung nicht klappt, aktiviere den Hotspot-Modus erneut über die Einstellungen.",
    "Fehler: WiFi SSID ist erforderlich!",

    "Display",
    "Helligkeit:",
    "Farbton",
    "Sättigung",
    "Sleep:",
    "in",
    "Nie",
    "Screensaver:",
    "Touch",
    "GT911 / kein IMU",

    "Leer",
    "Sensor",
    "Energie",
    "Wetter",
    "Szene",
    "Key",
    "Ordner",
    "Schalter",
    "Media",
    "Uhr",
    "Text",
    "Counter",
    "Settings",
    "Zurück",

    "Keine Auswahl",
    "Sensor Entity",
    "Einheit",
    "Nachkommastellen (leer = Originalwert)",
    "Wert-Größe",
    "Anzeige-Modus",
    "Keine",
    "Gauge",
    "Graph",
    "Gauge Min",
    "Gauge Max",
    "Bogengrad (90-359)",
    "Gauge Größe (100-800 px)",
    "Y-Offset (-100 bis 200)",
    "Graph Höhe (20-200 px)",
    "Popup öffnen",
    "Short Press",
    "Long Press",
    "Wert Y-Offset (-100 bis 200)",
    "Weather Entity",
    "Energie-Quelle",
    "Schalter/Licht",
    "Anzeige",
    "Icon Button",
    "LVGL Switch",
    "Media Player",
    "Uhrzeit anzeigen",
    "Datum anzeigen",
    "Uhrzeit Schriftgröße",
    "Datum Schriftgröße",
    "Text",
    "Text für die Kachel",
    "Text-Größe",
    "Max 31 Zeichen gespeichert.",
    "Ziel-Ordner",
    "Neuer Ordner",
    "Szene",
    "Makro",
    "Beispiele: g, ctrl+g, ctrl+shift+a",
    "Startwert",
    "Tap: +1, Long-Press: Reset auf 0",

    "Bitte zuerst eine Kachel wählen",
    "Kachel kopiert",
    "Keine kopierte Kachel vorhanden",
    "Kachel eingefügt",
    "Settings-Kachel (fest)",
    "Zurück-Kachel (fest)",
    "Diese Kachel kann nicht gelöscht werden",
    "Dieser Ordner kann nicht gelöscht werden",
    "Ordner \"{name}\" wirklich löschen?\n\nAlle Kacheln in diesem Ordner werden gelöscht und die Ordner-Kachel im übergeordneten Ordner wird entfernt.",
    "Ordner gelöscht",
    "Fehler beim Löschen",
    "Ordner nicht gefunden",
    "Kachel gespeichert & Display aktualisiert!",
    "Unbekannt",
    "Netzwerkfehler",
    "Netzwerkfehler beim Speichern",
    "Export erstellt!",
    "Export fehlgeschlagen",
    "Import-JSON ungültig",
    "Import fehlgeschlagen",
    "Import läuft...",
    "Import abgeschlossen!",
    "Kachel passt dort nicht hin",
    "Keine sinnvolle Anordnung gefunden",
    "Kacheln verschoben & gespeichert!",
    "Fehler beim Verschieben",
    "Netzwerkfehler beim Verschieben",
    "Screenshot wird erstellt...",
    "Screenshot gespeichert & Download gestartet!",
    "Screenshot fehlgeschlagen",
    "Bitte zuerst eine update.bin auswählen",
    "Firmware wird aktualisiert...",
    "Update wird installiert...",
    "Warte auf Neustart...",
    "Update erfolgreich installiert. Das Gerät startet jetzt neu.",
    "Firmware-Update fehlgeschlagen",

    // WLAN-Auswahl direkt am Geraet (Settings-Popup, siehe tab_settings.cpp)
    "Suche Netzwerke...",
    "Keine Netzwerke gefunden",
    "Neu suchen",
    "Manuell",
    "offen",
    "Passwort für %s",
    "Zurück",
    "Verbinden",
    "Gespeichert – Gerät startet neu...",
    "Speichern fehlgeschlagen",

    "Tastatur:",

    "Helligkeit, Standby & Rotation",
    "Netzwerk & Access Point",
    "Sprache, Zeitzone & Tastatur",
    "%s",

    "Drehen",

    "Gerät",
    "Nach Updates suchen",
    "Suche nach Updates...",
    "Firmware ist aktuell",
    "Update %s verfügbar",
    "2 Neustarts möglich. Danach Version prüfen.",
    "Auf %s aktualisieren",
    "Suche fehlgeschlagen",
    "Update wird geladen...",
    "Update fehlgeschlagen",
    "Installiert! Neustart...",

    "Trennen",
    "Koppeln",
    "Koppeln: MQTT verbindet neu...",

    "Bilder verwenden",
    "Zufällige Reihenfolge",
    "Bilder",
    "Anzeigedauer (Sekunden)",
    "Zoom",
    "Fokus X",
    "Fokus Y",
    "Uhrzeit",
    "Wochentag",
    "Textschatten",
    "Ausrichtung Uhrzeit",
    "Ausrichtung Datum",
    "Links",
    "Zentriert",
    "Rechts",
    "Kachel-Schatten",
    "Kachel-Rahmen",
    "Deckkraft",
    "Hintergrund oder Uhr anklicken. Kacheln in den beiden unteren Reihen lassen sich wie gewohnt verschieben und vergrößern.",
    "microSD erforderlich: Im Stammverzeichnis den Ordner /images erstellen und JPEG-Bilder dort ablegen.",
    "Keine JPEG-Dateien in /images - schwarzer Hintergrund bleibt aktiv.",
    "Screensaver gespeichert!",
    "Screensaver konnte nicht gespeichert werden",
    "Screensaver konnte nicht geladen werden",

    "Ethernet-Modus aktivieren",
    "WLAN-Modus aktivieren",
    "Netzwerkmodus geändert - gilt nach Neustart",
    "Ethernet statt WLAN (gilt nach Neustart)"};

static const Strings kStringsEn = {
    "en",
    "en",
    "Language:",
    "Time zone:",
    "Time format:",
    "Date format:",
    "Auto (language)",
    "Auto (localization)",
    "24-hour",
    "12-hour",
    "English",
    "Deutsch",

    "Home",
    "Folder ",

    "Waveshare Admin",
    "Waveshare Admin Panel",
    "Configuration & Overview",
    "Click a tile to edit it. Drag and drop tiles to move them, or use the corner handle to resize them. Choose the type (Sensor/Weather/Scene/Key/Folder/Settings/Switch/Media/Image/Clock/Text) and adjust its settings.",
    "Delete Folder / Tab",
    "Tile Settings",
    "Type",
    "Type locked: this folder still contains tiles. Empty it first – deleting the tile is still possible.",
    "Title",
    "Tile title",
    "Icon (MDI)",
    "e.g. home, thermometer, lightbulb",
    "Show icon list",
    "Color",
    "Column",
    "Row",
    "Width",
    "Height",
    "Changes are saved automatically.",
    "Copy",
    "Paste",
    "Delete",
    "Import / Export (all folders & tiles)",
    "Export",
    "Import",
    "Import overwrites the included folders/tiles and, when present, the screensaver.",
    "WiFi",
    "MQTT",
    "Localization",
    "Screenshot & Diagnostics",
    "Firmware Update",
    "Create & Download Screenshot",
    "Saves /ui_screenshot.jpg to the microSD card. The existing file is overwritten.",
    "Firmware file",
    "Current firmware",
    "Update",
    "Upload only the update.bin here. The factory.bin is only for the first flash.",
    "Choose file",
    "No file selected",

    "WiFi Status",
    "Connected",
    "Disconnected",
    "Offline",
    "AP active",
    "WiFi",
    "SSID",
    "IP",
    "Password",
    "Static IP",
    "Gateway",
    "Subnet mask",
    "DNS server",
    "Leave empty for DHCP",
    "MQTT not configured",
    "Enable AP",
    "Disable AP",
    "Yes",
    "No",

    "MQTT Host / IP",
    "Port",
    "Username",
    "Password",
    "MQTT Client ID",
    "empty = automatic",
    "Leave empty to generate it automatically from the MAC address.",
    "Device topic base",
    "Home Assistant prefix",
    "Time font size",
    "Date font size",
    "Save",
    "Restart device now?",
    "Restart",
    "MQTT configuration saved",
    "The device is reconnecting ...",
    "Bridge configuration saved",
    "Data was sent via MQTT.",
    "Save failed",

    "Waveshare WiFi Configuration",
    "WiFi Configuration",
    "Step 1: Connect to WiFi",
    "WiFi Connection",
    "SSID (network name)",
    "My WiFi",
    "Password",
    "Password",
    "Leave empty for an open network",
    "Note:",
    "After the WiFi connection works, you can configure MQTT later through the web interface on your normal network.",
    "Save & Connect",
    "Saved successfully!",
    "The configuration was saved successfully.<br>The device will now restart and try to connect to WiFi.",
    "You will be redirected automatically in 10 seconds.<br>If the connection fails, enable hotspot mode again from the settings.",
    "Error: WiFi SSID is required!",

    "Display",
    "Brightness:",
    "Hue",
    "Saturation",
    "Sleep:",
    "in",
    "Never",
    "Screensaver:",
    "Touch",
    "GT911 / no IMU",

    "Empty",
    "Sensor",
    "Energy",
    "Weather",
    "Scene",
    "Key",
    "Folder",
    "Switch",
    "Media",
    "Clock",
    "Text",
    "Counter",
    "Settings",
    "Back",

    "No selection",
    "Sensor Entity",
    "Unit",
    "Decimals (empty = original value)",
    "Value size",
    "Display mode",
    "None",
    "Gauge",
    "Graph",
    "Gauge Min",
    "Gauge Max",
    "Arc degree (90-359)",
    "Gauge size (100-800 px)",
    "Y offset (-100 to 200)",
    "Graph height (20-200 px)",
    "Open popup",
    "Short Press",
    "Long Press",
    "Value Y offset (-100 to 200)",
    "Weather Entity",
    "Energy Entity",
    "Switch / Light",
    "Display",
    "Icon Button",
    "LVGL Switch",
    "Media Player",
    "Show time",
    "Show date",
    "Time font size",
    "Date font size",
    "Text",
    "Text for the tile",
    "Text size",
    "Max 31 characters are stored.",
    "Target folder",
    "New folder",
    "Scene",
    "Macro",
    "Examples: g, ctrl+g, ctrl+shift+a",
    "Start value",
    "Tap: +1, long press: reset to 0",

    "Please select a tile first",
    "Tile copied",
    "No copied tile available",
    "Tile pasted",
    "Settings tile (fixed)",
    "Back tile (fixed)",
    "This tile cannot be deleted",
    "This folder cannot be deleted",
    "Delete folder \"{name}\"?\n\nAll tiles in this folder will be deleted and the folder tile in the parent folder will be removed.",
    "Folder deleted",
    "Delete failed",
    "Folder not found",
    "Tile saved & display updated!",
    "Unknown",
    "Network error",
    "Network error while saving",
    "Export created!",
    "Export failed",
    "Invalid import JSON",
    "Import failed",
    "Import in progress...",
    "Import complete!",
    "Tile does not fit there",
    "No valid arrangement found",
    "Tiles moved & saved!",
    "Move failed",
    "Network error while moving",
    "Creating screenshot...",
    "Screenshot saved & download started!",
    "Screenshot failed",
    "Please select an update.bin first",
    "Updating firmware...",
    "Installing update...",
    "Waiting for restart...",
    "Update installed successfully. The device is restarting now.",
    "Firmware update failed",

    // On-device WiFi selection (Settings popup, see tab_settings.cpp)
    "Scanning for networks...",
    "No networks found",
    "Scan again",
    "Manual",
    "open",
    "Password for %s",
    "Back",
    "Connect",
    "Saved - device is restarting...",
    "Saving failed",

    "Keyboard:",

    "Brightness, standby & rotation",
    "Network & access point",
    "Language, time zone & keyboard",
    "%s",

    "Rotate",

    "Device",
    "Check for updates",
    "Checking for updates...",
    "Firmware is up to date",
    "Update %s available",
    "May restart twice. Check version afterwards.",
    "Update to %s",
    "Check failed",
    "Downloading update...",
    "Update failed",
    "Installed! Restarting...",

    "Disconnect",
    "Pairing",
    "Pairing: reconnecting MQTT...",

    "Use images",
    "Shuffle",
    "Images",
    "Duration (seconds)",
    "Zoom",
    "Focus X",
    "Focus Y",
    "Clock",
    "Weekday",
    "Text shadow",
    "Time alignment",
    "Date alignment",
    "Left",
    "Centered",
    "Right",
    "Tile shadows",
    "Tile borders",
    "Opacity",
    "Click the background or clock. Tiles in the bottom two rows can be moved and resized as usual.",
    "microSD required: Create /images in the card root and place JPEG images there.",
    "No JPEG files in /images - the black background remains active.",
    "Screensaver saved!",
    "Could not save screensaver",
    "Could not load screensaver",

    "Enable Ethernet mode",
    "Enable WiFi mode",
    "Network mode changed - applies after restart",
    "Ethernet instead of WiFi (applies after restart)"};

static const LocaleProfile kLocaleDe = {
    "de",
    "Deutsch",
    ",",
    "Heute",
    "Morgen",
    {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"},
    {"Jan.", "Feb.", "Mär.", "Apr.", "Mai", "Jun.",
     "Jul.", "Aug.", "Sep.", "Okt.", "Nov.", "Dez."},
    {"Klare Nacht", "Bewölkt", "Ausnahme", "Nebel", "Hagel",
     "Gewitter", "Gewitterregen", "Teilw. bewölkt", "Starkregen",
     "Regen", "Schnee", "Schneeregen", "Sonnig", "Windig", "Böig"},
    "Klima",
    "Klima-Entity",
    "Solltemperatur",
    "Soll-Luftfeuchtigkeit",
    {"Heizbetrieb", "Vorheizen", "Kühlbetrieb", "Entfeuchtung", "Lüfter",
     "Abtauen", "Leerlauf", "Aus", "Heizen", "Kühlen", "Heizen/Kühlen",
     "Auto", "Entfeuchten", "Lüfter", "Klima"},
    {"Aktuell", "Soll-Temperatur", "Aktuelle Luftfeuchtigkeit"},
    {"Modus", "Voreinstellung", "Lüftermodus", "Oszillationsart",
     "Horizontale Oszillationsart"},
    {"Ohne", "Eco", "Abwesend", "Boost", "Komfort", "Zuhause", "Schlafen",
     "Aktivität", "Auto", "Niedrig", "Mittel", "Hoch", "An", "Aus",
     "Oben", "Mitte", "Fokus", "Verteilt", "Vertikal", "Horizontal",
     "Beide", "Links", "Mitte", "Rechts", "Schwenken", "Breit"}};

static const LocaleProfile kLocaleEn = {
    "en",
    "English",
    ".",
    "Today",
    "Tomorrow",
    {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"},
    {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"},
    {"Clear night", "Cloudy", "Exceptional", "Fog", "Hail",
     "Lightning", "Lightning rain", "Partly cloudy", "Pouring",
     "Rain", "Snow", "Sleet", "Sunny", "Windy", "Windy"},
    "Climate",
    "Climate Entity",
    "Target",
    "Target humidity",
    {"Heating", "Preheating", "Cooling", "Drying", "Fan", "Defrosting",
     "Idle", "Off", "Heat", "Cool", "Heat/Cool", "Auto", "Dry",
     "Fan only", "Climate"},
    {"Current", "Target temperature", "Current humidity"},
    {"Mode", "Preset", "Fan mode", "Swing mode",
     "Horizontal swing mode"},
    {"None", "Eco", "Away", "Boost", "Comfort", "Home", "Sleep",
     "Activity", "Auto", "Low", "Medium", "High", "On", "Off",
     "Top", "Middle", "Focus", "Diffuse", "Vertical", "Horizontal",
     "Both", "Left", "Center", "Right", "Swing", "Wide"}};

struct LanguageEntry {
  const Strings* strings;
  const LocaleProfile* locale;
};

static const LanguageEntry kLanguages[] = {
    {&kStringsEn, &kLocaleEn},
    {&kStringsDe, &kLocaleDe},
};

static const LanguageEntry& find_language(const char* language_code) {
  String code = language_code ? String(language_code) : String();
  code.trim();
  for (const LanguageEntry& language : kLanguages) {
    if (code.equalsIgnoreCase(language.locale->code)) return language;
  }
  return kLanguages[0];
}

const char* normalize_language_code(const char* language_code) {
  return find_language(language_code).locale->code;
}

const Strings& strings(const char* language_code) {
  return *find_language(language_code).strings;
}

const LocaleProfile& locale(const char* language_code) {
  return *find_language(language_code).locale;
}

String build_language_options_html(const char* selected_code) {
  const char* normalized = normalize_language_code(selected_code);
  String html;
  html.reserve(128);
  for (const LanguageEntry& language : kLanguages) {
    html += "<option value=\"";
    html += language.locale->code;
    html += "\"";
    if (strcmp(normalized, language.locale->code) == 0) html += " selected";
    html += ">";
    html += language.locale->native_name;
    html += "</option>";
  }
  return html;
}

String localize_numeric_text(
    const char* language_code, const String& numeric_text) {
  String text = numeric_text;
  text.trim();
  if (!text.length()) return numeric_text;

  String normalized = text;
  normalized.replace(",", ".");
  char* end = nullptr;
  const float value = strtof(normalized.c_str(), &end);
  if (end == normalized.c_str() || !isfinite(value)) return numeric_text;
  while (*end && isspace(static_cast<unsigned char>(*end))) ++end;
  if (*end) return numeric_text;

  if (locale(language_code).decimal_separator[0] == ',') {
    text.replace(".", ",");
  } else {
    text.replace(",", ".");
  }
  return text;
}

String format_number(
    const char* language_code,
    float value,
    uint8_t decimals,
    bool trim_trailing_zeros) {
  if (!isfinite(value)) return "--";
  const uint8_t digits = decimals > 6 ? 6 : decimals;
  String text(value, static_cast<unsigned int>(digits));
  if (trim_trailing_zeros && digits > 0) {
    while (text.endsWith("0")) text.remove(text.length() - 1);
    if (text.endsWith(".")) text.remove(text.length() - 1);
  }
  return localize_numeric_text(language_code, text);
}

static bool parse_iso_date_internal(const String& iso, int& y, int& m, int& d) {
  if (iso.length() < 10) return false;
  if (iso.charAt(4) != '-' || iso.charAt(7) != '-') return false;
  y = iso.substring(0, 4).toInt();
  m = iso.substring(5, 7).toInt();
  d = iso.substring(8, 10).toInt();
  return (y > 0 && m >= 1 && m <= 12 && d >= 1 && d <= 31);
}

String weather_condition_label(const char* language_code, const String& condition) {
  String key = condition;
  key.trim();
  key.toLowerCase();
  if (!key.length()) return "--";

  static const char* kKeys[] = {
      "clear-night", "cloudy", "exceptional", "fog", "hail",
      "lightning", "lightning-rainy", "partlycloudy", "pouring",
      "rainy", "snowy", "snowy-rainy", "sunny", "windy",
      "windy-variant"};
  const LocaleProfile& profile = locale(language_code);
  for (size_t i = 0; i < sizeof(kKeys) / sizeof(kKeys[0]); ++i) {
    if (key == kKeys[i]) return profile.weather_conditions[i];
  }

  String text = condition;
  text.replace("-", " ");
  text.replace("_", " ");
  text.trim();
  return text.length() ? text : "--";
}

String weather_weekday_short(const char* language_code, const String& iso) {
  int y = 0, m = 0, d = 0;
  if (!parse_iso_date_internal(iso, y, m, d)) return "";

  int mm = m;
  int yy = y;
  if (mm < 3) {
    mm += 12;
    yy -= 1;
  }
  int K = yy % 100;
  int J = yy / 100;
  int h = (d + (13 * (mm + 1)) / 5 + K + (K / 4) + (J / 4) + (5 * J)) % 7;
  int dow = (h + 6) % 7;
  if (dow < 0 || dow > 6) return "";

  return String(locale(language_code).weather_weekdays_short[dow]);
}

const char* weather_month_short(const char* language_code, int month) {
  if (month < 1 || month > 12) return "";
  return locale(language_code).weather_months_short[month - 1];
}

const char* weather_today_label(const char* language_code) {
  return locale(language_code).weather_today;
}

const char* weather_tomorrow_label(const char* language_code) {
  return locale(language_code).weather_tomorrow;
}

const char* climate_tile_type_label(const char* language_code) {
  return locale(language_code).tile_type_climate;
}

const char* climate_entity_label(const char* language_code) {
  return locale(language_code).climate_entity;
}

const char* climate_target_temperature_label(const char* language_code) {
  return locale(language_code).climate_target_temperature;
}

const char* climate_target_humidity_label(const char* language_code) {
  return locale(language_code).climate_target_humidity;
}

const char* climate_state_label(
    const char* language_code, const String& mode_value, const String& action_value) {
  String mode = mode_value;
  String action = action_value;
  mode.toLowerCase();
  action.toLowerCase();
  const LocaleProfile& profile = locale(language_code);
  if (action == "heating") return profile.climate_states[0];
  if (action == "preheating") return profile.climate_states[1];
  if (action == "cooling") return profile.climate_states[2];
  if (action == "drying") return profile.climate_states[3];
  if (action == "fan") return profile.climate_states[4];
  if (action == "defrosting") return profile.climate_states[5];
  if (action == "idle") return profile.climate_states[6];
  if (mode == "off" || action == "off") return profile.climate_states[7];
  if (mode == "heat") return profile.climate_states[8];
  if (mode == "cool") return profile.climate_states[9];
  if (mode == "heat_cool") return profile.climate_states[10];
  if (mode == "auto") return profile.climate_states[11];
  if (mode == "dry") return profile.climate_states[12];
  if (mode == "fan_only") return profile.climate_states[13];
  return profile.climate_states[14];
}

const char* climate_value_label(
    const char* language_code, uint8_t index) {
  if (index >= 3) return "";
  return locale(language_code).climate_value_labels[index];
}

const char* climate_control_label(
    const char* language_code, uint8_t index) {
  if (index >= 5) return "";
  return locale(language_code).climate_control_labels[index];
}

String climate_option_label(
    const char* language_code, const String& option_value) {
  static const char* const kKeys[] = {
      "none", "eco", "away", "boost", "comfort", "home", "sleep",
      "activity", "auto", "low", "medium", "high", "on", "off",
      "top", "middle", "focus", "diffuse", "vertical", "horizontal",
      "both", "left", "center", "right", "swing", "wide"};
  String key = option_value;
  key.trim();
  key.toLowerCase();
  if (key == "heat" || key == "cool" || key == "heat_cool" ||
      key == "dry" || key == "fan_only") {
    return climate_state_label(language_code, key, "");
  }
  const LocaleProfile& profile = locale(language_code);
  for (uint8_t i = 0; i < 26; ++i) {
    if (key == kKeys[i]) return profile.climate_option_labels[i];
  }
  key.replace("_", " ");
  key.replace("-", " ");
  if (key.length()) {
    key.setCharAt(0, static_cast<char>(
        toupper(static_cast<unsigned char>(key.charAt(0)))));
  }
  return key;
}

}  // namespace i18n

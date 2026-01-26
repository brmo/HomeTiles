# Neuer Tile-Typ (vollstaendiger Leitfaden)

Dieser Guide beschreibt **alle Schritte** fuer einen neuen Tile-Typ.
Ziel: keine vergessenen Stellen, klarer Ablauf, sauberer Build.

---

## 0) Schreibweisen festlegen

Beispiel: `weather`
- GROSS: `WEATHER`
- CamelCase: `Weather`
- klein: `weather`

Du brauchst alle drei Varianten (Dateinamen, Funktionen, Konstanten).

---

## 1) Schnellstart per Python-Script (empfohlen)

Script:
```
python scripts/create_tile_type.py weather
```

Das Script macht:
- kopiert `templates/tile_type_template/` nach `src/types/weather`
- ersetzt Platzhalter `TEMPLATE` / `Template` / `template`
- gibt eine kurze Next-Steps-Liste aus

Das Script macht **nicht**:
- Enum/Registry/Web-Admin aendern
- neue Felder in den zentralen Handlern verdrahten

---

## 2) Template: Was du wirklich aendern musst

Das Template ist **eine funktionierende Basis**, aber du musst es anpassen:

Pflicht:
- Ordnername + Includes auf deinen Typ
- Funktionsnamen (render/apply/web)
- Beispiel-Feldnamen (`template_value`, `TemplateFields`, CSS-Klasse)

Merke: Das Script ersetzt **3 Platzhalter**, aber **alle Beispiel-Feldnamen**
musst du selbst sinnvoll benennen.

Typische Aenderungen im Template:
- `template_value` -> `weather_city`
- `append_template_fields_html` -> `append_weather_fields_html`
- CSS `.tile.template` -> `.tile.weather`

---

## 3) Schritt-fuer-Schritt (manuell, ohne Script)

### 3.1 Ordner anlegen
Kopiere Template:
`templates/tile_type_template/` -> `src/types/<name>/`

Ersetze:
- `TEMPLATE` -> `WEATHER`
- `Template` -> `Weather`
- `template` -> `weather`

### 3.2 Enum erweitern
Datei: `src/tiles/tile_config.h`
```
TILE_WEATHER = <freie_id>
```

### 3.3 Renderer deklarieren
Datei: `src/tiles/tile_renderer.h`
```
lv_obj_t* render_weather_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index);
```

### 3.4 Renderer implementieren
Datei: `src/types/<name>/renderer.cpp`

Pflicht:
- `lv_button_create(...)`
- `set_tile_grid_cell(card, col, row, tile.span_w, tile.span_h)`
- Farben/Rundung/Shadow setzen
- Optional: Icon + Title rendern

Hinweis:
Fonts kommen aus `src/tiles/tile_renderer_fonts.h`.

### 3.5 Web-HTML fuer Felder
Datei: `src/types/<name>/web_html.cpp`

Regeln:
- Container-ID: `<tab>_<fields_suffix>_fields`
- Feld-IDs: `<tab>_<feldname>`

### 3.6 Web-Scripts (Load/Save/Reset)
Datei: `src/types/<name>/web_scripts.cpp`

Pflichtfunktionen:
- `load<Name>Fields(tab, data)`
- `save<Name>Fields(tab, formData)`
- `reset<Name>Fields(tab)`

Wichtig:
Die FormData-Keys muessen exakt zu den Keys passen, die der Web-Handler liest.

### 3.7 Web-Handler (Apply)
Datei: `src/types/<name>/web_handler.cpp`

Pflicht:
- Werte aus `server.arg("...")` lesen
- in `tile.<feld>` speichern
- alte Felder zuruecksetzen, damit keine Altwerte bleiben

### 3.8 Registry eintragen
Datei: `src/types/types_registry.cpp`

Du musst:
1) Includes hinzufuegen (renderer + web_*)
2) Wrapper-Funktionen erstellen (render/apply/append)
3) Eintrag in `kTileTypes` anlegen

Beispiel-Eintrag:
```
{
  TILE_WEATHER,
  "Weather",
  "weather",
  "weather",
  "none",
  nullptr,
  "loadWeatherFields",
  "saveWeatherFields",
  "resetWeatherFields",
  0x353535,
  false,
  render_weather_wrapper,
  apply_weather_wrapper,
  append_weather_fields_wrapper,
  append_weather_styles,
  append_weather_scripts
},
```

Erklaerung:
- `css_class`: CSS-Klasse fuer Preview
- `fields_suffix`: muss zu `<tab>_<fields_suffix>_fields` passen
- `preview_kind`: `none`, `sensor`, `switch`, `clock`, `text` oder eigener Wert
- `js_on_select`: optionaler JS Hook beim Typwechsel

### 3.9 Web-Admin verdrahten (wichtig)
Datei: `src/web/web_admin_scripts.cpp`

Pflicht, wenn du neue Felder hast:
1) In `setupLivePreview(...)`:
   - Feld-ID in `fields` aufnehmen
   - Event-Listener setzen (updatePreview + autosave)
2) In `postTile(...)`:
   - deine Felder in FormData appenden (Import/Export)
3) In `updateTilePreview(...)`:
   - falls Preview deine neuen Felder braucht
4) In `renderTileFromData(...)`:
   - falls Preview deinen neuen Typ darstellen soll

Wenn du nur `title/icon/bg` nutzt, reichen 1) und 2).

### 3.10 Optional: Hint-Text
Datei: `src/web/web_admin_html.cpp`
Nur noetig, wenn du die Typ-Liste im Info-Text pflegen willst.

---

## 4) Datenfluss: Speichern + Aktualisieren (sehr wichtig)

### 4.1 Web-Flow (Standard)
1) Web-UI sammelt Felder -> `save<Name>Fields(...)`
2) `POST /api/tiles` -> `WebAdminServer::handleSaveTiles()`
3) `desc->apply(...)` ruft `apply_<name>_fields_from_request(...)`
4) `tileConfig.saveFolderGrid(...)`
5) `tiles_invalidate_folder(folder_id)`
6) falls aktiv: `tiles_request_reload_if_loaded(GridType::TAB0)`

### 4.2 Runtime-Updates (ohne Web)
Wenn dein Tile zur Laufzeit etwas speichert:
- Grid laden, Tile aendern
- `tileConfig.saveFolderGrid(...)`
- `tiles_invalidate_folder(...)`
- ggf. `tiles_request_reload_if_loaded(...)`

### 4.3 MQTT-Updates (Live)
LVGL darf **nicht** im MQTT-Callback aktualisiert werden.
Nutze Queue/Flags und update im Main Loop.

Beispiel:
- `queue_sensor_tile_update(...)`
- `process_sensor_update_queue()` im Loop

---

## 5) Zusatzfunktionen (optional, aber haeufig)

### 5.1 Neue Felder (Checkliste)
Wenn du ein neues Feld hinzufuegst:
1) HTML Input (ID)
2) JS load/save/reset
3) Web-Handler apply
4) Live-Preview wiring
5) Import/Export (postTile)

### 5.2 MQTT (eingehend)
Entity-basiert:
- `rebuildDynamicRoutes(...)`
- `tiles_update_sensor_by_entity(...)`

Eigene Topics:
- `mqtt_topics.*` + `mqtt_handlers.cpp`
- Update ueber Queue, nicht direkt LVGL

### 5.3 MQTT (ausgehend)
Nutze vorhandene Publish-Helpers oder erweitere `mqtt_handlers.cpp`.

### 5.4 WebSocket
Beispiele:
- `src/game/game_ws_server.*`
- `src/types/key/renderer.cpp`

### 5.5 HTTP/URL
Beispiel:
- `src/ui/image_popup.cpp`

Regel:
Kein Netzwerk im LVGL-Event blockieren.

### 5.6 Popups
Beispiele:
- `src/ui/sensor_popup.*`
- `src/ui/light_popup.*`

### 5.7 Lange Daten auf SD
Wenn Felder zu klein sind:
- eigene Dateien auf SD
- nur Pfad/ID im Tile speichern

---

## 6) Wichtige Limits (damit nix kaputt geht)

- `Tile` Felder sind fest gepackt.
- Neue Felder im `Tile` bedeuten:
  Update der Packed-Struktur + Save/Load + evtl. Migration.
- Besser: vorhandene Felder wiederverwenden oder SD nutzen.

---

## 7) Testablauf

1) Typ erscheint im Dropdown
2) Felder sichtbar + editierbar
3) Live-Preview reagiert
4) Speichern klappt
5) Neustart laedt korrekt
6) Display zeigt Layout + Inhalt

---

## 8) Kurz-Checkliste

- [ ] Template kopiert / Script genutzt
- [ ] Enum in `tile_config.h`
- [ ] Renderer in `tile_renderer.h`
- [ ] Typ-Ordner fertig
- [ ] Registry-Eintrag in `types_registry.cpp`
- [ ] Web-Admin wiring in `web_admin_scripts.cpp`
- [ ] Build + Test

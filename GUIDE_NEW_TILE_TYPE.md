# Neuer Tile-Typ (ausfuehrlicher Leitfaden)

Dieser Guide beschreibt **alle Schritte** und typische Erweiterungen
(neue Felder, MQTT, WebSocket, URL/HTTP, Popups). Ziel: Kein Schritt
geht verloren.

---

## Schnelluebersicht: Pflichtdateien

Pflicht:
- `src/tiles/tile_config.h` (Enum erweitern)
- `src/tiles/tile_renderer.h` (Renderer deklarieren)
- `src/types/<name>/...` (neuer Typ-Ordner)
- `src/types/types_registry.cpp` (Registry + Wrapper)
- `src/web/web_admin_scripts.cpp` (Live-Preview + Import/Export)

Optional:
- `src/web/web_admin_html.cpp` (Hint-Text)

---

## 0) Typ-Name festlegen

Beispiel: `weather`
- Gross: `WEATHER`
- CamelCase: `Weather`
- Klein: `weather`

Diese drei Schreibweisen brauchst du.

---

## 1) Python-Script benutzen (empfohlen)

Script:
`scripts/create_tile_type.py`

Beispiel:
```
python scripts/create_tile_type.py weather
```

Das Script:
- kopiert das Template nach `src/types/weather`
- ersetzt `TEMPLATE` / `Template` / `template`
- schreibt einen Hinweis zu den naechsten manuellen Schritten

Hinweis:
Das Script **aendert keine zentralen Dateien**.
Registry/Enum/Web-Admin musst du weiterhin selbst anpassen.

---

## 2) Template manuell kopieren (Alternative)

Template-Ordner:
`templates/tile_type_template/`

Kopiere nach:
`src/types/<name>/`

Danach **alle Platzhalter ersetzen**:
- `TEMPLATE`  -> `WEATHER`
- `Template`  -> `Weather`
- `template`  -> `weather`

Wichtig:
In den Includes steht `src/types/template/...` und muss auf deinen
Ordner angepasst werden.

---

## 3) Daten festlegen (wo speichern?)

Die `Tile`-Struktur hat feste Felder (Packed-Storage).
Diese solltest du wiederverwenden.

Limits:
- title: 32
- icon_name: 32
- sensor_entity: 64
- sensor_unit: 16
- scene_alias: 32
- key_macro: 32

Regeln:
- `TILE_SENSOR` nutzt `scene_alias` intern fuer Gauge-Infos.
  Dieses Feld dort nicht fuer eigene Daten verwenden.
- `TILE_IMAGE` speichert Pfad auf SD (nicht im Packed).
- Fuer laengere Daten: SD oder neue Packed-Version.

---

## 4) Enum erweitern

Datei: `src/tiles/tile_config.h`

Neuen Typ hinzufuegen:
```
TILE_<NAME> = <freie_id>
```

---

## 5) Renderer deklarieren

Datei: `src/tiles/tile_renderer.h`

Neue Funktion:
```
lv_obj_t* render_<name>_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index);
```

---

## 6) Typ-Ordner ausfuellen

Ordner: `src/types/<name>/`

Pflichtdateien:
- `renderer.h/.cpp`
- `web_html.h/.cpp`
- `web_styles.h/.cpp`
- `web_scripts.h/.cpp`
- `web_handler.h/.cpp`

Was dort passiert:

### A) renderer.cpp
- Tile erstellen (`lv_button_create`)
- Farbe/Radius/Shadow setzen
- `set_tile_grid_cell(...)`
- Optional: Icon + Title
- Eigene Anzeige-Logik
- Events (Click/Long-Press) registrieren

### B) web_html.cpp
- HTML-Block fuer Einstellungen erzeugen
- IDs muessen dem Pattern folgen:
  `<tab>_<feldname>`
- Block-ID muss sein:
  `<tab>_<fields_suffix>_fields`

### C) web_scripts.cpp
Pflichtfunktionen:
- `load<Name>Fields(tab, data)`
- `save<Name>Fields(tab, formData)`
- `reset<Name>Fields(tab)`

### D) web_handler.cpp
- Werte aus `server.arg("...")` lesen
- In `tile.<feld>` speichern
- Sensor-Standardwerte zuruecksetzen (damit keine alten Werte bleiben)

### E) web_styles.cpp
- CSS fuer Preview (`.tile.<css_class>`)

---

## 7) Registry eintragen

Datei: `src/types/types_registry.cpp`

Du musst:
1) Includes hinzufuegen
2) Wrapper-Funktionen anlegen
3) Eintrag in `kTileTypes`

Beispiel:
```
{
  TILE_MYTYPE,
  "My Type",
  "mytype",
  "mytype",
  "none",
  nullptr,
  "loadMyTypeFields",
  "saveMyTypeFields",
  "resetMyTypeFields",
  0x353535,
  false,
  render_mytype_wrapper,
  apply_mytype_wrapper,
  append_mytype_fields_wrapper,
  append_mytype_styles,
  append_mytype_scripts
},
```

Erklaerung:
- `css_class`: CSS Klasse fuer Preview
- `fields_suffix`: muss zu `<tab>_<fields_suffix>_fields` passen
- `preview_kind`: `none`, `sensor`, `switch`, `clock` oder eigener Wert
- `locked`: feste Tiles (z.B. Settings/Back)

---

## 8) Web-Admin zentral verdrahten

Datei: `src/web/web_admin_scripts.cpp`

### A) Live-Preview + Autosave
In `setupLivePreview(...)`:
- Feld-IDs in `fields` Array aufnehmen
- Event-Listener anhaengen

### B) Preview Rendering
Wenn `preview_kind` neu ist:
- `updateTilePreview(...)` erweitern
- `renderTileFromData(...)` erweitern

### C) Import/Export
In `postTile(...)` deine Felder in `FormData` appenden.

---

## 9) Optional: Hint-Text

Datei: `src/web/web_admin_html.cpp`

Nur wenn die Typ-Liste im Hinweistext aktualisiert werden soll.

---

## 10) Zusatzfunktionen (ausfuehrlich)

### 10.1 Neue Felder (mehr Inputs)
1) Feld in `web_html.cpp` anlegen (ID: `<tab>_<feld>`)
2) `load<Name>Fields` / `save<Name>Fields` / `reset<Name>Fields` erweitern
3) `apply_<name>_fields_from_request` erweitert das `Tile`
4) `setupLivePreview(...)` ergaenzen
5) `postTile(...)` fuer Import/Export ergaenzen

---

### 10.2 MQTT (eingehend)

Es gibt zwei typische Wege:

**A) Dynamische Sensor-Updates (entity-basiert)**  
Wenn dein Typ Daten wie Sensoren bekommen soll:
- in `src/network/mqtt_handlers.cpp` die Funktion
  `rebuildDynamicRoutes(...)` erweitern, damit dein Typ aufgenommen wird
- in `src/ui/tab_tiles_unified.cpp` die Funktion
  `tiles_update_sensor_by_entity(...)` erweitern, damit dein Typ Updates bekommt
- im Renderer ein Widget speichern und per Queue updaten

**B) Eigene feste Topics**  
Wenn dein Typ eigene MQTT-Topics hat:
- `src/network/mqtt_topics.{h,cpp}` -> neuen TopicKey eintragen
- `src/network/mqtt_handlers.cpp` -> Route + Handler eintragen
- Handler darf **kein LVGL direkt** anfassen:
  nutze Queue-Mechanismen oder Flags, die im Loop verarbeitet werden

---

### 10.3 MQTT (ausgehend)

Bestehende Helfer:
- `mqttPublishScene(...)`
- `mqttPublishSwitchCommand(...)`
- `mqttPublishLightCommand(...)`

Wenn du neue Kommandos brauchst:
- neue TopicKeys + Publish-Funktion in `mqtt_handlers.cpp`
- im Tile-Event diese Publish-Funktion aufrufen

---

### 10.4 WebSocket

Beispiel:
- `src/game/game_ws_server.*`
- init in `Tab5_LVGL.ino` (Port 8081)
- Verwendung in `src/types/key/renderer.cpp`

Wenn du WebSocket fuer deinen Typ willst:
- eigene Broadcast-Funktion oder vorhandenen Server nutzen
- im Renderer bei Click/Long-Press senden

---

### 10.5 HTTP/URL (Daten von URL holen)

Beispiel:
- `src/ui/image_popup.cpp` (HTTPClient + HTTPS)

Hinweise:
- Netz-Requests nicht im LVGL-Event blockieren
- Daten zwischenspeichern (cache) und dann UI aktualisieren

---

### 10.6 Popups

Beispiele:
- `src/ui/sensor_popup.*`
- `src/ui/light_popup.*`

Pattern:
- im Renderer Event-Handler fuer Click/Long-Press
- Popup-Funktion aufrufen (z.B. `show_sensor_popup(...)`)
- optional Queue-Update nutzen (thread-safe)

---

### 10.7 Speicherung auf SD (lange Texte, Assets)

Wenn Packed-Fields zu klein sind:
- eigene Dateien auf SD
- Pfad + Index z.B. wie bei `TILE_IMAGE` in `tile_config.cpp`
- bei Load/Save lesen/schreiben

---

## 11) Testablauf

- Typ erscheint im Web-Dropdown
- Felder werden sichtbar
- Live-Preview reagiert
- Speichern klappt
- Neustart uebersteht
- Display zeigt korrekt

---

## 12) Kurz-Checkliste

- [ ] Script oder Template genutzt
- [ ] Enum erweitert (`tile_config.h`)
- [ ] Renderer deklariert (`tile_renderer.h`)
- [ ] Typ-Ordner fertig
- [ ] Registry Eintrag (`types_registry.cpp`)
- [ ] Web-Admin Wiring (`web_admin_scripts.cpp`)
- [ ] Build + Test

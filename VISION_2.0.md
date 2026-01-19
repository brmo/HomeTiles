# Tab5 LVGL 2.0 - Zielbild aus dem Ist-Code

Dieses Dokument beschreibt eine 2.0-Struktur, abgeleitet aus dem aktuellen Code.
Ziel: klare Rollen, weniger Hardcoding, maximale Modularitaet, neue Tile-Typen
ohne Flash, und trotzdem schnell und stabil.

## 1) Ist-Stand (Code-Realitaet)
- UI Shell: `src/ui/ui_manager.cpp`
  - Sidebar + 4 Tabs (Tab0/Tab1/Tab2/Settings)
  - feste Groessen: 1280x720, `SIDEBAR_WIDTH=180`
- Tile Grid: `src/ui/tab_tiles_unified.cpp`
  - 3x4 Raster, `GAP=24`, `GRID_PAD=24`
  - Tile-Position = Index (row = i/3, col = i%3)
- Tile Renderer: `src/tiles/tile_renderer.cpp`
  - Typ-spezifische Widgets und Layout
- Tile Config + NVS: `src/tiles/tile_config.*`
  - Speicherung pro Grid in NVS (Preferences)
  - Bilder via Pfad, geladen von SD
- Web Admin: `src/web/web_admin_*`
  - Tiles bearbeiten, Tabs benennen, Reorder (Swap)
- WiFi Setup: `src/web/web_config.*` (AP Captive Portal)
- Netzwerk/HA/MQTT: `src/network/*`
- Game WS Server: `src/game/game_ws_server.*` (Port 8081)
- Electron App: `electron-app/main.js`
  - PC Metrics, SimConnect Bridge, Key Output
  - WS Verbindung zum Device (8081)
  - startet `simconnect-bridge` (C#)

## 2) Probleme, die 2.0 loesen muss
- Layout und Sidebar sind hardcoded (Groessen, Raster, Tabs).
- Tile Groessen sind fix, kein 2x1/2x2 usw.
- Neue Tile Typen brauchen Firmware-Flash.
- Settings sind ein Sonderfall (fixer Tab, nicht Tile-basiert).
- Reorder ist Swap, kein echtes Drag/Resize.
- UI und Datenquellen sind nicht sauber getrennt.

## 3) 2.0 Ziele (Leitlinien)
- Neue Tile Typen ohne Flash (XML + Registry + Upload).
- Layout frei skalierbar (z.B. 7x5 Grid, freie Spans).
- Einheitliches Tile-Objekt: Type + Layout + Props + Bindings + Actions + Popup.
- Device funktioniert ohne PC.
- Electron App bleibt PC Bridge (SimConnect, PC Metrics, Key Output).
- Web Admin ist zentrale Konfig fuer Layout/Assets/Bindings.

## 4) Rollen in 2.0
### Device (ESP32-P4)
- LVGL Renderer + XML Loader.
- Layout Engine (Grid + Spans).
- Data Bus (MQTT/WS/Local) und Action Bus.
- Speichert Config (NVS + LittleFS).
- Fallback Settings Screen fuer Erstsetup.

### Web Admin (Device)
- Layout Editor (Drag/Resize/Order).
- Tile Instanzen verwalten (Props/Bindings/Actions).
- XML/Assets Upload.

### Electron App (PC Bridge)
- PC Datenquellen: SimConnect + PC Metrics.
- Key Output (Button Press -> Keyboard).
- Keine UI-Konfiguration fuer das Device.

## 5) Systembild (bildlich)
```
           +-------------------------------+
           | Electron App (PC Bridge)      |
           | - SimConnect                  |
           | - PC Metrics                  |
           | - Key Output                  |
           +---------------+---------------+
                           |
                           | WS (data + actions)
                           v
+-----------------------------------------------------+
| DEVICE (ESP32-P4)                                   |
| - LVGL UI + XML Components                          |
| - Layout Engine (grid + spans)                      |
| - Data Bus / Action Bus                             |
| - Storage: NVS + LittleFS                           |
| - MQTT/HA + Web Admin                               |
+-----------------------------------------------------+
                           ^
                           |
                           | HTTP/REST (config + upload)
                           |
           +---------------+---------------+
           | Web Admin (Device UI)         |
           | - Layout Editor               |
           | - Tile Config                 |
           | - Upload XML/Assets           |
           +-------------------------------+
```

## 6) Boot- und Laufzeitablauf
1) NVS laden (WiFi, MQTT, Display)
2) Wenn unconfigured: AP + Captive Portal
3) XML + Layout aus LittleFS laden
4) LVGL UI bauen
5) Data Bus starten (MQTT, WS, Local)
6) Web Admin bereitstellen

## 7) Datenmodell (Type, Instance, Layout)
### 7.1 Tile Type (Registry)
Die Registry definiert, welche XML Components es gibt und welche Props/Bindings
ein Typ versteht. Damit kann der Web Admin validieren und das Device rendern.

Beispiel `types/registry.json`:
```
{
  "types": [
    {
      "id": "ha_sensor",
      "component": "/ui/components/ha_sensor.xml",
      "popup": "/ui/components/ha_sensor_popup.xml",
      "props": ["title", "icon", "unit", "decimals"],
      "bindings": ["value"],
      "actions": ["tap", "long_press"],
      "default_size": [1, 1]
    }
  ]
}
```

### 7.2 Tile Instance (Layout JSON)
Tile Instanzen enthalten Layout und konkrete Konfiguration.

Beispiel `layout/layout.json`:
```
{
  "grid": { "cols": 7, "rows": 5, "gap": 12, "pad": 12 },
  "tiles": [
    {
      "id": "tile_01",
      "type": "ha_sensor",
      "x": 0, "y": 0, "w": 2, "h": 1,
      "props": {
        "title": "Wohnzimmer",
        "icon": "mdi:thermometer",
        "unit": "C",
        "decimals": 1
      },
      "bindings": {
        "value": "data.ha.sensor.temp_living"
      },
      "actions": {
        "tap": "action.ha.toggle_lamp"
      },
      "popup": "ha_sensor_popup"
    }
  ]
}
```

### 7.3 Grid-Regeln
- `x,y,w,h` sind Grid-Einheiten, nicht Pixel.
- XML definiert keine Groesse, es rendert responsiv im Tile-Container.

## 8) Layout Engine
- Grid-Groesse pro Screen definierbar (z.B. 7x5).
- Tile-Spans: 1x1, 2x1, 1x2, 2x2 usw.
- Web Admin drag/resize schreibt Layout JSON.
- Konflikte: entweder blockieren (kein Overlap) oder auto-fix (naechster freier Platz).

## 9) XML Integration (LVGL)
- XML beschreibt nur UI-Struktur und Styles.
- XML wird beim Start geladen, danach ist nur LVGL-Objektbaum im RAM.
- Keine XML-DOMs dauerhaft halten.
- Globals (Styles, Fonts, Icons) in `globals.xml`.
- Vorteil: neue Typen ohne Flash (nur XML + Registry + Assets).

Wichtig: Ohne Flash sind nur neue UI-Typen moeglich. Neue Protokolle oder
Treiber brauchen weiterhin Firmware-Updates.

## 10) Data Bus + Action Bus
Tiles kennen nur Keys, keine Protokolle.

Beispiele Data Keys:
- data.ha.sensor.temperature
- data.sim.airspeed
- data.pc.cpu_load
- data.system.time

Beispiele Action Keys:
- action.ha.toggle_light
- action.sim.pause
- action.pc.keypress
- action.nav.goto
- action.system.open_settings

### Quellen
- MQTT/HA: Werte rein, Actions raus.
- WS (Electron): PC Metrics und SimConnect rein, Key Output raus.
- Local: Time/Power/WiFi direkt vom Device.

## 11) Speicher und Update
### 11.1 NVS (Preferences)
Nur Device-Config:
- WiFi SSID/Pass
- MQTT Host/User/Pass/Base Topic
- Display/Power

### 11.2 LittleFS (Dateien)
Alles ohne Flash updatebar:
```
/ui/globals.xml
/ui/components/*.xml
/ui/screens/*.xml
/layout/layout.json
/types/registry.json
/assets/fonts/*
/assets/icons/*
/assets/images/*
```

### 11.3 Update Pipeline
- Upload via Web Admin (einzeln oder ZIP).
- In temp schreiben, validieren, dann atomar umschalten.
- Optionales Rollback (letzte Version behalten).

## 12) Settings / WiFi / Recovery
- Erstsetup immer ueber Captive Portal (AP).
- Settings als normales Tile im Layout (typ "settings").
- Fallback: Long-Press auf Statusbar oder Hardware-Button oeffnet Settings,
  falls kein Tile existiert.

## 13) Performance und Speicher
- PSRAM fuer Draw Buffer und groessere Assets.
- Fonts als Subsets, aber austauschbar (Icon-Packs).
- Bilder streamen aus FS, keine Voll-Buffer im RAM.
- XML nur parsen, nicht in RAM halten.

## 14) Migration (realistisch)
1) Neues Layout-JSON + Grid-Engine (noch ohne XML).
2) Registry + XML Loader integrieren.
3) Data/Action Bus einfuehren.
4) Web Admin Layout Editor + Uploads.
5) Bestehende Tile Typen nach XML portieren.

## 15) Offene Entscheidungen (festlegen bevor Coding startet)
- Grid-Default (z.B. 7x5) und Abstaende.
- Welche Datenquellen sind "Core" (MQTT, WS, Local).
- Wie "Popup" in XML standardisiert wird.
- Format des Upload-Bundles (ZIP, Signatur, Version).
- Wie gross die LittleFS Partition sein soll.

## ESP32-P4 Home Assistant Display

<img align="right" width="38%" src="docs/images/b4-home.png" alt="Waveshare B4 home dashboard">

Tile-based ESP32-P4 firmware for Home Assistant dashboards with a fully configurable web interface.

The project supports multiple ESP32-P4 touch displays and combines:

- touch-first, tile-based dashboard UI
- MQTT-based Home Assistant integration
- on-device settings: WiFi setup, display, language, firmware updates
- firmware updates directly on the device (GitHub releases) or via the web interface
- full dashboard configuration through the built-in web admin panel

<br clear="both">

## Requirements

- Home Assistant
- MQTT broker
- The Home Assistant bridge/integration:
  [ESP32-P4 HomeAssistant Display Bridge](https://github.com/GalusPeres/ESP32-P4-HomeAssistant-Display-Bridge)

## Highlights Of The v0.2.x Releases

- All three supported devices are now covered by every release
- Firmware updates directly from the device: Settings → System checks GitHub for new releases and installs them over the air
- Reworked on-device settings: WiFi network scan with on-screen keyboard, Access Point mode with QR code, display/brightness/sleep options, language and time settings, restart button
- Major rendering performance improvements on the M5Stack Tab5 and the Waveshare 8" display (hardware-accelerated rotation, faster draw paths)
- General UI polish across tiles and popups

<!-- TODO screenshots: Settings-Kachelseite + System-Popup mit Update-Button, WLAN-Popup (Netzliste), AP-Ansicht mit QR -->

## Overview

This firmware turns supported ESP32-P4 touch displays into configurable Home Assistant control panels.

Everything visible on the dashboard is tile-based and managed from the built-in web interface:
- add, remove, move, and resize tiles
- drag and drop tiles between positions directly in the web interface
- configure tile content and behavior
- create folders and navigation structures
- manage WiFi, MQTT, language, and time zone settings without changing code

## Supported Devices

- [M5Stack Tab5](https://shop.m5stack.com/products/m5stack-tab5-iot-development-kit-esp32-p4)
- [Waveshare ESP32-P4-WIFI6-Touch-LCD-4B](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4b.htm)
- [Waveshare ESP32-P4-WIFI6-Touch-LCD-8 (8 inch)](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-7-8-10.1.htm)

Device-specific Arduino IDE settings are documented in [BOARD_SETTINGS.md](BOARD_SETTINGS.md).

## Screenshots

The screenshots below were captured on the Waveshare B4. They are meant as example views of the UI; the same firmware and web admin panel run on all supported devices.

<!-- TODO: Screenshots sind vom alten UI-Stand - neue Aufnahmen folgen (Home 8-Zoll + B4/Tab5, Settings, System-Update, WLAN, Energy Tag+Woche, Licht, Wetter, Web-Admin) -->

### Main Views

Home dashboard, folder view, and settings screen:

<img src="docs/images/b4-home.png" alt="Home dashboard" width="32%"> <img src="docs/images/b4-folder-lights.png" alt="Folder view" width="32%"> <img src="docs/images/b4-settings.png" alt="Settings view" width="32%">

### Popups

Sensor history views:

<img src="docs/images/b4-sensor-popup-water-24h.png" alt="Sensor popup water 24 hour view" width="32%"> <img src="docs/images/b4-sensor-popup-water-7d.png" alt="Sensor popup water 7 day view" width="32%"> <img src="docs/images/b4-weather-popup.png" alt="Weather popup" width="32%">

Light control views:

<img src="docs/images/b4-light-popup-brightness-off.png" alt="Light popup on off view" width="24%"> <img src="docs/images/b4-light-popup-brightness.png" alt="Light popup brightness view" width="24%"> <img src="docs/images/b4-light-popup-color.png" alt="Light popup color view" width="24%"> <img src="docs/images/b4-light-popup-temperature.png" alt="Light popup temperature view" width="24%">

### Web Admin

Built-in web admin interface for tiles, folders, WiFi, MQTT, and layout configuration:

<p>
  <img src="docs/images/web-admin.png" alt="Web admin interface" width="100%">
</p>

## Features

- Firmware updates directly on the device (checks GitHub releases, installs over the air)
- OTA firmware upload from the built-in web admin panel
- Fully tile-configurable dashboard via the built-in web admin panel
- Drag-and-drop tile layout editing in the web admin panel
- MQTT-based Home Assistant communication
- On-device WiFi setup: network scan with on-screen keyboard, or Access Point mode with QR code
- On-device settings for display brightness, sleep, orientation, language, time zone, and time format
- English and German UI/admin support, 12h/24h time formats
- Home Assistant energy statistics tile with day and week popup charts
- Media player tile with cover art and playback controls
- microSD file manager in the web admin (upload, download, rename, delete, folders)
- Runtime storage on internal LittleFS; microSD is optional
- Screenshot export to microSD from the web interface
- Tile types currently include:
  - clock
  - counter
  - energy
  - empty
  - key
  - media
  - navigate
  - scene
  - sensor
  - switch
  - text
  - weather

## Installation

### Option 1: Prebuilt Binaries

Download the files matching your device from the [latest release](https://github.com/GalusPeres/ESP32-P4-HomeAssistant-Display/releases/latest):

| Device | First flash | OTA update file |
| --- | --- | --- |
| M5Stack Tab5 | `...-m5stacks_tab5-factory.bin` | `...-m5stacks_tab5.bin` |
| Waveshare 4B | `...-waveshare_4b-factory.bin` | `...-waveshare_4b.bin` |
| Waveshare 8" | `...-waveshare_touch_lcd_8-factory.bin` | `...-waveshare_touch_lcd_8.bin` |

Use:
- `factory.bin` for a clean first flash (ESP Flash Download Tool at address `0x00000`)
- the plain `.bin` for OTA updates of an existing device (web admin upload)

A manual reset after flashing may be required.

### Option 2: Update From The Device

Devices already running a recent v0.2.x firmware can update themselves:
open `Settings` → `System` → check for updates. The device finds the
latest GitHub release and installs it directly.

### Option 3: Build From Source

1. Open [ESP32_P4_HomeAssistant_Display.ino](ESP32_P4_HomeAssistant_Display.ino) in the Arduino IDE.
2. Select the target device in [src/devices/device_select.h](src/devices/device_select.h).
3. Apply the correct board settings from [BOARD_SETTINGS.md](BOARD_SETTINGS.md).
4. Build and flash the firmware.

## First Setup

1. Flash the firmware and boot the device.
2. Open `Settings` → `WLAN` on the device. Either:
   - pick your network from the scan list and enter the password with the on-screen keyboard, or
   - enable Access Point mode: connect to the device hotspot (password `12345678`, QR code shown on screen) and enter your WiFi credentials in the captive portal.
3. After saving, the device restarts and connects to your WiFi network.
4. The device IP address is shown in the on-device WLAN settings.
5. Open the web admin panel through that IP address.
6. Enter your MQTT settings in the web interface.
7. Set up the Home Assistant bridge/integration so the device receives entity data:
   [ESP32-P4 HomeAssistant Display Bridge](https://github.com/GalusPeres/ESP32-P4-HomeAssistant-Display-Bridge)
8. Configure your tiles, folders, and layout.

Optional:
- Insert a FAT32-formatted microSD card if you want to use the file manager or screenshot export from the web interface.

## Home Assistant Integration

This firmware expects the Home Assistant side to be provided by the MQTT bridge/integration:

- [ESP32-P4 HomeAssistant Display Bridge](https://github.com/GalusPeres/ESP32-P4-HomeAssistant-Display-Bridge)

That integration handles the Home Assistant-side MQTT communication and entity bridge.
For Energy tiles, Home Assistant energy statistics, live icon updates, and popup history, use the current bridge release.

## Repository Structure

- `src/` firmware source code
- `docs/images/` screenshots and documentation images
- `electron-app/` desktop companion tooling
- `mdi-extractor/` icon tooling
- `simconnect-bridge/` additional companion tooling
- `BOARD_SETTINGS.md` documented Arduino IDE board settings

## Known Issues

- M5Stack Tab5: Access Point mode is currently only reliable with a battery installed. Without a battery, keep brightness at the lowest available level; otherwise the device can crash.

## Notes

- A microSD card is not required for normal operation; it is only used for the web file manager and screenshot export.
- Board selection and board settings must match the target device.
- A Windows Electron companion app also exists under `electron-app/`. It can be used to send PC-side data to the device, for example Microsoft Flight Simulator values, system metrics, or simulated keyboard input/commands for Windows. This still needs proper documentation and its own release packaging.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).

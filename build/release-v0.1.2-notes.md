# ESP32-P4 Home Assistant Display v0.1.2

Supported devices:
- M5Stacks Tab5
- Waveshare B4

Highlights:
- OTA firmware updates via the web interface
- improved OTA restart and display handling on both supported devices
- shared OTA-compatible partition layout for Tab5 and Waveshare B4

Included binaries:
- `esp32-p4-homeassistant-display-v0.1.2-m5stacks-tab5-factory.bin`
- `esp32-p4-homeassistant-display-v0.1.2-m5stacks-tab5-update.bin`
- `esp32-p4-homeassistant-display-v0.1.2-waveshare-b4-factory.bin`
- `esp32-p4-homeassistant-display-v0.1.2-waveshare-b4-update.bin`

Notes:
- `factory.bin` is intended for first installation / clean flash.
- `update.bin` is intended for updating an existing installation.
- `factory.bin` should be flashed at offset `0x00000`.
- `update.bin` should be flashed at offset `0x10000`.
- OTA updates can now be installed from the web interface using the matching `update.bin`.
- When using the ESP Flash Download Tool, a manual reset after flashing may be required.
- A Home Assistant setup with MQTT and the bridge integration is required.
- A microSD card formatted as FAT32 is required.

Bridge integration:
- https://github.com/GalusPeres/ha-tab5-mqtt-bridge

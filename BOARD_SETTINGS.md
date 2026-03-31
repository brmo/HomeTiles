# Board Settings

This file documents the working Arduino IDE board settings for the two device profiles used in this project.

Note:
- The remaining values should be set exactly as listed below.

## M5Stacks Tab5

Used for:
- `src/devices/m5stacks_tab5`

Important:
- Leave `Partition Scheme` on the normal Tab5 default.
- Do not use the repo `partitions.csv` for Tab5 builds.

Arduino IDE:
- Board: `M5Tab5`
- USB CDC On Boot: `Enabled`
- CPU Frequency: `360MHz`
- Core Debug Level: `None`
- USB DFU On Boot: `Disabled`
- Erase All Flash Before Sketch Upload: `Disabled`
- Flash Frequency: `80MHz`
- Flash Mode: `QIO`
- Flash Size: `16MB (128Mb)`
- JTAG Adapter: `Disabled`
- USB Firmware MSC On Boot: `Disabled`
- Partition Scheme: `Default (2 x 6.5 MB app, 3.6 MB SPIFFS)`
- PSRAM: `Enabled`
- Upload Mode: `UART0 / Hardware CDC`
- Upload Speed: `921600`
- USB Mode: `Hardware CDC and JTAG`

## Waveshare B4

Used for:
- `src/devices/waveshare_4b`

Important:
- For B4 OTA builds, use the repo-root `partitions.csv`.
- In Arduino IDE set `Partition Scheme` to `Custom`.
- This keeps both OTA app slots fully below `16MB`.

Arduino IDE:
- Board: `ESP32P4 Dev Module`
- USB CDC On Boot: `Disabled`
- CPU Frequency: `360MHz`
- Core Debug Level: `None`
- USB DFU On Boot: `Disabled`
- Erase All Flash Before Sketch Upload: `Disabled`
- Flash Frequency: `80MHz`
- Flash Mode: `QIO`
- Flash Size: `32MB (256Mb)`
- JTAG Adapter: `Disabled`
- USB Firmware MSC On Boot: `Disabled`
- Partition Scheme: `Custom`
- PSRAM: `Enabled`
- Upload Mode: `UART0 / Hardware CDC`
- Upload Speed: `921600`
- USB Mode: `USB-OTG (TinyUSB)`

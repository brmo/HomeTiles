# Firmware Updates

There are three ways to get firmware onto a device. For normal operation you only ever
need the first one.

## 1. On-Device Updater (recommended)

Open **Settings → System** on the display and tap the update check button. The device
looks up the latest [GitHub release](https://github.com/GalusPeres/HomeTiles/releases/latest),
and if a newer version exists, offers to install it. The download and installation run
directly on the device with a progress bar; afterwards it restarts into the new version.

Notes:
- The device briefly disconnects from MQTT and stops the web admin during the install —
  both come back automatically.
- If the install fails (network hiccup, crash), nothing is lost: the device keeps running
  the old version. Just try again.

## 2. Web Admin OTA Upload

Open the web admin panel (`http://<display-ip>/`), go to the OTA section, and upload the
update binary for your device manually. The screen turns off during the installation —
this is intentional (it frees memory for the transfer) and the device restarts when done.

Use the asset matching your device from the release page:

| Device | OTA update file |
| --- | --- |
| M5Stack Tab5 | `hometiles_<version>_m5stacks_tab5.bin` |
| Waveshare 4B | `hometiles_<version>_waveshare_4b.bin` |
| Waveshare 8" | `hometiles_<version>_waveshare_touch_lcd_8.bin` |

Older devices still running v0.2.9 or earlier look for the previous
`esp32-p4-homeassistant-display-<version>-<device>-update.bin` naming; the on-device
updater falls back to it automatically if a release doesn't have the current-named asset.

## 3. Factory Flash (first installation / full reset)

For a brand-new device or a full reset, flash the `-factory.bin` image — it's a
complete flash image (its file size matches the chip's full flash size), so writing it
at address `0x00000` wipes and reinstalls everything: bootloader, app, and the stored
WiFi/MQTT/tile configuration. No separate erase step is needed.

Using the ESP Flash Download Tool (`ESP32P4 FLASH DOWNLOAD TOOL`):

1. ChipType: `ESP32-P4`, WorkMode: `Develop`, LoadMode: `UART` → OK.
2. Select the `-factory.bin` matching your device, address `0x0`.
3. Pick the device's COM port (device connected via USB) and leave BAUD at `115200`.
4. Click **START**. A manual reset after flashing may be required.

**If you're resetting an already-paired device:** delete its entry in Home Assistant
*before* expecting a new "discovered device" card to appear. The device's unique ID is
derived from its MAC address, which survives a full flash wipe — Home Assistant treats
the ID as already configured and won't show a fresh discovery card until the old entry
is removed.

## Building From Source

1. Open `HomeTiles.ino` in the Arduino IDE.
2. Select the target device in `src/devices/device_select.h`.
3. Apply the board settings from [BOARD_SETTINGS.md](https://github.com/GalusPeres/HomeTiles/blob/master/BOARD_SETTINGS.md).
4. Build and flash.

The firmware version comes from `version.txt`. The on-device updater compares this
version against the latest release tag, and expects release assets to follow the naming
scheme shown above.

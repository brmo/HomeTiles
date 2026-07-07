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
| M5Stack Tab5 | `hometiles-<version>-m5stacks_tab5.bin` |
| Waveshare 4B | `hometiles-<version>-waveshare_4b.bin` |
| Waveshare 8" | `hometiles-<version>-waveshare_touch_lcd_8.bin` |

Older devices still running v0.2.9 or earlier look for the previous
`esp32-p4-homeassistant-display-<version>-<device>-update.bin` naming; recent releases
still include both until no devices on that old firmware remain.

## 3. Factory Flash (first installation)

For a brand-new device or a full reset, flash the `-factory.bin` image with the
ESP Flash Download Tool at address `0x00000`. A manual reset after flashing may be
required. This wipes the stored configuration.

## Building From Source

1. Open `HomeTiles.ino` in the Arduino IDE.
2. Select the target device in `src/devices/device_select.h`.
3. Apply the board settings from [BOARD_SETTINGS.md](https://github.com/GalusPeres/HomeTiles/blob/master/BOARD_SETTINGS.md).
4. Build and flash.

The firmware version comes from `version.txt`. The on-device updater compares this
version against the latest release tag, and expects release assets to follow the naming
scheme shown above.

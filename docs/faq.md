# FAQ & Troubleshooting

## Tiles show no data

Work through this checklist:

1. Is the entity selected in the bridge integration options? (**Settings → Devices &
   Services → HomeTiles Bridge → Configure → Entity Configuration**)
2. Are the display's MQTT settings correct (host, port, credentials)? Check the web admin.
3. Do the base topic and HA prefix match between the display and the integration entry?
4. Is the panel listed as a device under the bridge integration? If not, tap
   **Settings → System → Pairing** on the display — it re-announces itself.

More details in the [Home Assistant Setup Guide](home-assistant-setup.md).

## The display is missing in Home Assistant / I deleted it there

Tap **Settings → System → Pairing** on the display: it reconnects MQTT and republishes
its discovery data, and the device reappears under the bridge integration. If you
deleted the device in Home Assistant, do that first — Home Assistant ignores discovery
from device IDs it still knows.

## The display briefly flashes blue when saving or updating (Waveshare)

Cosmetic and harmless. The display panel is refreshed continuously from PSRAM; while the
firmware writes to internal flash (saving tile edits, installing updates), that refresh
briefly stalls and the panel shows a solid color. The underlying fix is an ESP-IDF build
option (`CONFIG_SPIRAM_XIP_FROM_PSRAM`) that the precompiled Arduino core does not enable,
so it currently cannot be fixed from this project's code.

## The Tab5 dims itself when enabling AP mode or after a reboot

Intentional. Full backlight plus a WiFi radio burst can trip the Tab5's brownout detector
(hard reset). Since v0.2.9 the firmware caps the backlight around these moments and
restores your configured brightness automatically once WiFi is connected.

## Ghost images / shadows of previous content (Waveshare)

Temporary LCD image retention, typical for these panels: static high-contrast content
(white text on dark tiles) leaves faint ghosts, most visible on grey surfaces and on a
cold panel. It fades as the display warms up and is not permanent burn-in. A shorter
display sleep timeout reduces it.

## The display crashed or restarted by itself

The firmware records crash diagnostics automatically: after a crash, the next boot
appends the reset reason and a summary (crashed task, program counter, registers) to a
crash log, and the full crash state is kept in flash as a core dump. Please report it —
these files are exactly what's needed to find and fix the bug:

1. Open the web admin panel and go to **Screenshot & Diagnostics**.
2. Click **Download crash log** to get `crashlog.txt`.
3. If a stored core dump is shown, download it too and **pack the `.bin` into a
   `.zip`** — GitHub does not accept raw `.bin` attachments.
4. [Open an issue](https://github.com/GalusPeres/HomeTiles/issues) describing what the
   display was doing when it crashed (which firmware version, which popup or action, how
   often it happens), and attach both files by dragging them into the issue text box.

!!! note "What's in a core dump?"
    A snapshot of the firmware's working memory at the moment of the crash. It can
    contain things currently on screen or in memory — tile titles, entity names, sensor
    values. If you'd rather not share that, attach only the crash log; it already
    narrows down the crash location.

## The screen goes black during a web admin OTA upload

Intentional — the display is suspended during the transfer to free memory. The device
restarts when the installation finishes. The on-device updater (Settings → System) keeps
the screen on and shows a progress bar instead.

## The update check says "up to date" but I expected an update

The device compares its own version (shown in Settings → System) against the latest
GitHub release tag. If a release was just published, wait a moment and check again. If an
install fails repeatedly, download the OTA file and use the web admin upload instead —
see [Firmware Updates](updating.md).

## MQTT disconnects during updates

Intentional. Update installs temporarily disconnect MQTT and stop the web admin to free
memory. Everything reconnects automatically afterwards; Home Assistant data resyncs on
reconnect.

## AP mode basics

- Password: `12345678` (shown with a QR code on the display)
- The access point switches itself off after 10 minutes without a saved configuration
- While AP mode is active, MQTT and the web admin are unavailable

## What happens if Home Assistant or the MQTT broker is offline?

The display keeps running and retries the broker every few seconds in the background;
the UI stays fully responsive. Once the broker is reachable again, it reconnects and
resyncs all entity states automatically.

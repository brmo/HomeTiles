# FAQ & Troubleshooting

## Tiles show no data

Work through this checklist:

1. Is the entity selected in the bridge integration options? (**Settings → Devices &
   Services → HomeTiles Bridge → Configure → Entity Configuration**)
2. Are the display's MQTT settings correct (host, port, credentials)? Check the web admin.
3. Do the base topic and HA prefix match between the display and the integration entry?
4. Is the panel listed as a device under the bridge integration? If not, restart the
   display once — it announces itself on every connect.

More details in the [Home Assistant Setup Guide](home-assistant-setup.md).

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

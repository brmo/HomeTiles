# Home Assistant Setup Guide

This guide walks you through connecting an ESP32-P4 HomeAssistant Display to Home Assistant,
starting from scratch. No YAML editing is required.

## How It Works

```
Display  <-- MQTT -->  MQTT Broker  <-- MQTT -->  Bridge Integration (Home Assistant)
```

The display never talks to Home Assistant directly. Everything goes through MQTT:

- The **bridge integration** pushes entity states, icons, weather forecasts, sensor history,
  and energy data to the display.
- The display sends commands back (light/switch/media/scene control), which the bridge
  executes in Home Assistant.

So you need three things: an **MQTT broker**, the **bridge integration**, and the
**display firmware**. This guide covers all three.

## Step 1: Set Up An MQTT Broker

If you already have a working MQTT broker, skip to Step 2.

The easiest option on Home Assistant OS is the official Mosquitto add-on:

1. In Home Assistant, go to **Settings → Add-ons → Add-on Store**.
2. Search for **Mosquitto broker**, install it, and start it.
   Enable **Start on boot** and **Watchdog**.
3. Create a Home Assistant user for the display:
   **Settings → People → Users → Add user** (for example `display`, with a password of
   your choice). The Mosquitto add-on accepts Home Assistant users as MQTT credentials.
4. Set up the MQTT integration: after the add-on starts, Home Assistant usually discovers
   it automatically — go to **Settings → Devices & Services**, look for the discovered
   **MQTT** integration, and confirm it. If it is not discovered, add the **MQTT**
   integration manually and use `core-mosquitto` as the broker address.

## Step 2: Install The Bridge Integration

The bridge is a custom integration: [ESP32-P4 HomeAssistant Display Bridge](https://github.com/GalusPeres/ESP32-P4-HomeAssistant-Display-Bridge)

### Via HACS (recommended)

1. **HACS → Integrations → three-dot menu → Custom repositories**
2. Repository: `https://github.com/GalusPeres/ESP32-P4-HomeAssistant-Display-Bridge`,
   category **Integration**, click **Add**.
3. Search for **ESP32-P4 HomeAssistant Display Bridge** in HACS and download it.
4. Restart Home Assistant.

### Manual

1. Copy the `custom_components/tab5_lvgl` folder from the bridge repository into your
   Home Assistant `custom_components` directory.
2. Restart Home Assistant.

## Step 3: Add The Integration

1. Go to **Settings → Devices & Services → Add Integration**.
2. Search for **ESP32-P4 HomeAssistant Display Bridge**.
3. Keep the defaults unless you have a reason not to:
   - **Base topic**: `tab5` — must match the base topic configured on the display
     (the firmware default is also `tab5`).
   - **HA prefix**: `ha/statestream` — must also match the display (same default on both sides).
   - Optionally set a device name so you can tell your panels apart.

## Step 4: Connect The Display To The Broker

1. Get the display onto your WiFi (on-device: **Settings → WLAN**, pick your network and
   enter the password — or use Access Point mode, see the [README](../README.md#first-setup)).
2. Open the display's web admin panel in a browser: `http://<display-ip>/`
   (the IP is shown in the on-device WLAN settings).
3. Enter the MQTT settings:
   - **Host**: the IP address of your Home Assistant machine (when using the Mosquitto add-on,
     the broker runs there).
   - **Port**: `1883`
   - **Username / Password**: the Home Assistant user you created in Step 1.
   - Leave **base topic** and **HA prefix** at their defaults unless you changed them in Step 3.
4. Save. The display restarts and connects to the broker.

Once connected, the display announces itself and the bridge links it to the integration
entry automatically. You should see the panel appear as a device under
**Settings → Devices & Services → ESP32-P4 HomeAssistant Display Bridge**.

## Step 5: Choose What The Display Can See

Open the integration entry and click **Configure**. There are three sections:

- **Panel Settings** — base topic, HA prefix, and device metadata.
- **Entity Configuration** — pick the sensors, weather entities, lights, switches,
  media players, and scenes/scripts the display should have access to. Scene aliases are
  generated automatically; you can also map them manually (one `alias=entity_id` per line).
- **Energy Dashboard** — enable electricity, gas, and/or water. This requires the
  Home Assistant [Energy Dashboard](https://my.home-assistant.io/redirect/energy/) to be
  set up; the display's energy tile pulls its statistics from there.

Entity selections are shared across all panels — every display can use every entity you
pick here.

## Step 6: Build Your Dashboard

Back in the display's web admin panel, add tiles and assign the entities you exposed in
Step 5 (sensor tiles, light/switch tiles, weather, energy, media, scenes, and so on).
Changes appear on the display immediately.

## Multiple Displays

- Each display needs its **own base topic** (for example `tab5`, `panel_kitchen`, ...) —
  set it in the display's MQTT settings.
- The **HA prefix stays the same** for all displays.
- Additional displays are discovered automatically: once the first panel is set up, any new
  display that connects to the broker gets its own integration entry without manual steps.

## Troubleshooting

**The display connects to WiFi but shows no data**
- Check the MQTT settings on the display (host, port, credentials).
- Make sure base topic and HA prefix match between the display and the integration entry.
- Check **Settings → Devices & Services** — the panel should be listed under the bridge
  integration. If not, restart the display once; it re-announces itself on every connect.

**Lights/switches/sensors are missing on the display**
- They must be selected in the bridge options first (Step 5), then assigned to a tile in
  the web admin (Step 6).

**The energy tile stays empty**
- The Home Assistant Energy Dashboard must be configured, and the matching category
  (electricity/gas/water) must be enabled in the bridge's Energy options.

**MQTT login fails**
- When using the Mosquitto add-on, the credentials are a regular Home Assistant user
  (Step 1), not an add-on-specific account.

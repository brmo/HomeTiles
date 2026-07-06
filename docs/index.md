# ESP32-P4 HomeAssistant Display

Tile-based firmware that turns ESP32-P4 touch displays into Home Assistant control
panels — configured entirely in the browser, updated over the air, connected via MQTT.

![Home dashboard](images/b4-home.png){ width="65%" }

<div class="grid cards" markdown>

-   :material-rocket-launch:{ .lg .middle } **Get started**

    ---

    MQTT broker, bridge integration, and connecting your first display — step by step

    [:octicons-arrow-right-24: Home Assistant Setup](home-assistant-setup.md)

-   :material-transit-connection-variant:{ .lg .middle } **Bridge integration**

    ---

    Panel settings, entity configuration, scene aliases, and the MQTT topic reference

    [:octicons-arrow-right-24: Bridge Integration](bridge.md)

-   :material-view-grid-plus:{ .lg .middle } **Build your dashboard**

    ---

    Every tile type: sensors, lights, scenes, weather, energy, media, and more

    [:octicons-arrow-right-24: Tile Types](tiles.md)

-   :material-update:{ .lg .middle } **Stay up to date**

    ---

    On-device updater, web OTA upload, and factory flashing

    [:octicons-arrow-right-24: Firmware Updates](updating.md)

-   :material-help-circle:{ .lg .middle } **Something not working?**

    ---

    Common questions and known quirks, explained honestly

    [:octicons-arrow-right-24: FAQ & Troubleshooting](faq.md)

-   :material-github:{ .lg .middle } **Source & releases**

    ---

    MIT-licensed firmware and bridge, prebuilt binaries for every device

    [:octicons-arrow-right-24: GitHub](https://github.com/GalusPeres/ESP32-P4-HomeAssistant-Display)

</div>

## Supported Devices

| Device | Display |
| --- | --- |
| [M5Stack Tab5](https://shop.m5stack.com/products/m5stack-tab5-iot-development-kit-esp32-p4) | 5" 1280×720 |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-4B](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4b.htm) | 4" 720×720 |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-8](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-7-8-10.1.htm) | 8" 1280×800 |

The same firmware runs on all devices; every release ships prebuilt binaries for each.

## How It Works

```
Display  <-- MQTT -->  MQTT Broker  <-- MQTT -->  Bridge Integration (Home Assistant)
```

The display never talks to Home Assistant directly. The bridge integration pushes entity
states, icons, weather, history, and energy data over MQTT — and executes the commands
the display sends back.

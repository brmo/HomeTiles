# ESP32-P4 HomeAssistant Display

Tile-based firmware that turns ESP32-P4 touch displays into Home Assistant control
panels — configured entirely from a built-in web interface, updated over the air,
connected via MQTT.

![Home dashboard](images/b4-home.png)

## Supported Devices

| Device | Display |
| --- | --- |
| [M5Stack Tab5](https://shop.m5stack.com/products/m5stack-tab5-iot-development-kit-esp32-p4) | 5" 1280×720 |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-4B](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4b.htm) | 4" 720×720 |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-8](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-7-8-10.1.htm) | 8" 1280×800 |

The same firmware runs on all devices; releases ship prebuilt binaries for each.

## How It Works

```
Display  <-- MQTT -->  MQTT Broker  <-- MQTT -->  Bridge Integration (Home Assistant)
```

The display never talks to Home Assistant directly. A companion integration — the
[bridge](bridge.md) — pushes entity states, icons, weather, history, and energy data
over MQTT, and executes the commands the display sends back.

## Where To Start

1. **Flash a device** — grab the `-factory.bin` for your device from the
   [latest release](https://github.com/GalusPeres/ESP32-P4-HomeAssistant-Display/releases/latest)
   (details: [Firmware Updates](updating.md))
2. **Connect it to Home Assistant** — follow the
   [Home Assistant Setup Guide](home-assistant-setup.md)
3. **Build your dashboard** — open the web admin at `http://<display-ip>/` and add
   [tiles](tiles.md)

Something not working? Check the [FAQ](faq.md).

## Links

- [GitHub repository](https://github.com/GalusPeres/ESP32-P4-HomeAssistant-Display)
- [Releases](https://github.com/GalusPeres/ESP32-P4-HomeAssistant-Display/releases)
- [Bridge integration repository](https://github.com/GalusPeres/ESP32-P4-HomeAssistant-Display-Bridge)

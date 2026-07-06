# Tile Types

Everything on the dashboard is a tile on a grid. Tiles are created, moved, resized, and
configured in the web admin panel (`http://<display-ip>/`). Tiles that show Home Assistant
data need their entity to be exposed through the bridge integration first — see the
[Home Assistant Setup Guide](home-assistant-setup.md), Step 5.

Most data tiles have a popup with more detail. Whether the popup opens on a **tap** or a
**long press** is configurable per tile in the web admin.

## Home Assistant Tiles

### Sensor
Shows the current value of any Home Assistant entity, with icon, label, and unit.
The popup shows a history chart (24-hour and 7-day view); the history data is fetched
live from Home Assistant through the bridge.

### Switch
Toggles a `switch` or `light` entity with a tap. For lights, the popup provides full
light control: brightness, color, and color temperature.

### Scene
Triggers a scene or script. The tile references the **scene alias** defined in the bridge
integration (aliases are generated automatically when you select scenes/scripts in the
bridge options, or mapped manually there as `alias=entity_id`).

### Weather
Shows current conditions from a `weather` entity. The popup contains the daily and hourly
forecast provided by the bridge.

### Energy
Shows statistics from the Home Assistant **Energy Dashboard** (electricity, solar, grid,
battery, gas, or water). The popup charts a day view (hourly) and a week view (daily).
Requires the matching energy category to be enabled in the bridge options, and a
configured Energy Dashboard in Home Assistant.

### Media
Controls a `media_player` entity: cover art, title/artist, and playback controls.
The popup provides the full control set including volume.

## Local Tiles

### Clock
Time and date. Follows the device's localization settings (language, time zone,
12h/24h format); the format can also be overridden per tile.

### Text
A static text tile — useful for headings and labels on the grid.

### Counter
A simple tap counter: tap to count up, long-press to reset.

### Folder
Opens a sub-page with its own tile grid. A back tile is placed on the sub-page
automatically. Use folders to group lights, rooms, or feature areas.

### Key
Sends a key/button command to PC clients connected to the display's built-in WebSocket
server (port 8081). This works together with the desktop companion app (`electron-app/`)
to trigger keyboard input or commands on a Windows PC — it is not related to
Home Assistant.

### Empty
A spacer tile for layout purposes.

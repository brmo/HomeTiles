# SimConnect Bridge

Small WebSocket bridge that reads MSFS SimConnect data and streams it to the Electron app.

## Setup
1) Install the MSFS SDK (Developer Mode -> SDK -> Install).
2) Copy the DLLs from the SDK:
   - Managed: `.../MSFS SDK/SimConnect SDK/lib/managed/Microsoft.FlightSimulator.SimConnect.dll`
     -> `simconnect-bridge/lib/Microsoft.FlightSimulator.SimConnect.dll`
   - Native: `.../MSFS SDK/SimConnect SDK/lib/SimConnect.dll`
     -> `simconnect-bridge/lib/SimConnect.dll`
3) Build the bridge:

```
cd simconnect-bridge
dotnet build
```

## Run
```
cd simconnect-bridge
dotnet run
```

Options:
- `--host 127.0.0.1`
- `--port 13375`
- `--rate 10` (Hz)
- `--config C:\path\to\simconnect-bridge-config.json`

The bridge serves WebSocket on `ws://127.0.0.1:13375` and emits messages like:
```
{
  "type": "sim_metrics",
  "timestamp": 1730000000000,
  "sensors": [
    {"entity_id":"sim.ias","name":"IAS","unit":"kt","value":120.5}
  ]
}
```

## Publish (for Electron packaging)
```
powershell -ExecutionPolicy Bypass -File scripts/publish.ps1
```

This outputs to `simconnect-bridge/publish/` for `electron-builder` extraResources.

Default sensors (configurable via `--config`):
- `sim.ias` (INDICATED AIRSPEED, knots)
- `sim.tas` (AIRSPEED TRUE, knots)
- `sim.mach` (AIRSPEED MACH, mach)
- `sim.gs` (GPS GROUND SPEED, knots)
- `sim.altitude` (PLANE ALTITUDE, feet)
- `sim.altitude_indicated` (INDICATED ALTITUDE, feet)
- `sim.vs` (VERTICAL SPEED, feet per minute)
- `sim.heading` (PLANE HEADING DEGREES MAGNETIC, degrees)
- `sim.heading_true` (PLANE HEADING DEGREES TRUE, degrees)
- `sim.fuel_total` (FUEL TOTAL QUANTITY, gallons)
- `sim.on_ground` (SIM ON GROUND, bool)

Config file format:
```
{
  "version": 1,
  "simvars": [
    { "entity_id": "sim.ias", "name": "Indicated Airspeed", "simvar": "INDICATED AIRSPEED", "unit": "knots", "type": "float64" }
  ]
}
```

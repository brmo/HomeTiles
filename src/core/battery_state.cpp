#include "src/core/battery_state.h"

// Waveshare ESP32-P4-WIFI6-Touch-LCD-4B has no battery / PMIC.
// All functions return "on mains, no battery" defaults.

static BatteryTelemetry g_stub = {
  .on_mains = true,
  .battery_missing = true,
  .level_valid = false,
  .level_pct = -1,
  .raw_level_pct = -1,
  .voltage_mv = 0,
  .current_ma = 0,
  .vbus_mv = 5000,
  .has_vbus = true,
  .charging = false,
};

void batteryStateUpdate() {
  // nothing to poll
}

const BatteryTelemetry& batteryStateGet() {
  return g_stub;
}

bool batteryStateIsOnMains() {
  return true;
}

bool batteryStateIsBatteryMissing() {
  return true;
}

bool batteryStateHasDisplayPercent() {
  return false;
}

int32_t batteryStateDisplayPercent() {
  return -1;
}

int32_t batteryStateRawPercent() {
  return -1;
}

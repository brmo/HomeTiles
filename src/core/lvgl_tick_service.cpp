#include "lvgl_tick_service.h"

#include <Arduino.h>
#include <lvgl.h>

uint32_t g_lvgl_tick_last_ms = 0;

// Mindestabstand zwischen zwei echten Service-Durchlaeufen. Oefter zu
// rendern, als das Display Frames anzeigen kann, ist reine Verschwendung:
// gemessen hat eine dichte Pump-Schleife (parseEnergySection, 19 Eintraege
// mit je <1ms Parse-Arbeit dazwischen) 811ms nur mit Rendern verbracht --
// jeder Aufruf fand die Animation "faellig", weil der VORHERIGE Render
// selbst laenger als die Frame-Periode dauerte. 15ms traegt bis ~60fps;
// uebersprungene Aufrufe verlieren keine Zeit (der geteilte Tick-Zeitstempel
// wird nur bei echten Services fortgeschrieben, der Rest wird beim naechsten
// Service bzw. am Loop-Anfang nachgezaehlt).
static constexpr uint32_t kMinServiceIntervalMs = 15;

void lvglServiceDuringBlockingWork() {
  uint32_t now = millis();
  uint32_t elapsed = now - g_lvgl_tick_last_ms;
  if (elapsed < kMinServiceIntervalMs) {
    yield();  // Watchdog weiter fuettern, wie bisher bei jedem Aufruf
    return;
  }
  lv_tick_inc(elapsed);
  g_lvgl_tick_last_ms = now;
  yield();
  lv_timer_handler();
  yield();
}

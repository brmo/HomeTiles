#include "lvgl_tick_service.h"

#include <Arduino.h>
#include <lvgl.h>

uint32_t g_lvgl_tick_last_ms = 0;

void lvglServiceDuringBlockingWork() {
  uint32_t now = millis();
  lv_tick_inc(now - g_lvgl_tick_last_ms);
  g_lvgl_tick_last_ms = now;
  yield();
  lv_timer_handler();
  yield();
}

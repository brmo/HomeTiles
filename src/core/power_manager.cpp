#include "src/core/power_manager.h"
#include <M5Unified.h>
#include "src/core/display_manager.h"
#include "src/core/config_manager.h"
#include "src/network/network_manager.h"
#include "src/network/mqtt_handlers.h"
#if defined(ARDUINO_ARCH_ESP32)
#include "esp32-hal-cpu.h"
#endif

PowerManager powerManager;

void PowerManager::applyCpuFrequency(uint16_t mhz) {
#if defined(ARDUINO_ARCH_ESP32)
  if (mhz == 0) return;
  if (last_cpu_mhz == 0) {
    last_cpu_mhz = static_cast<uint16_t>(getCpuFrequencyMhz());
  }
  if (last_cpu_mhz == mhz) return;
  setCpuFrequencyMhz(static_cast<uint32_t>(mhz));
  last_cpu_mhz = static_cast<uint16_t>(getCpuFrequencyMhz());
  Serial.printf("CPU Freq -> %u MHz\n", static_cast<unsigned>(last_cpu_mhz));
#else
  (void)mhz;
#endif
}

void PowerManager::init() {
  Serial.println("⚡ Init Power Manager...");
  disp = displayManager.getDisplay();
  is_high_performance = true;
#if defined(ARDUINO_ARCH_ESP32)
  last_cpu_mhz = static_cast<uint16_t>(getCpuFrequencyMhz());
#endif
}

void PowerManager::setHighPerformance(bool enable) {
  if (enable == is_high_performance) return;
  is_high_performance = enable;

  if (enable) {
    applyCpuFrequency(CPU_FREQ_HIGH);
    if (isPoweredByMains()) networkManager.setWifiPowerSaving(false);

#if LV_VERSION_MAJOR >= 9
    if (disp) {
        lv_timer_t * rt = lv_display_get_refr_timer(disp);
        if(rt) lv_timer_set_period(rt, 1000 / FPS_HIGH);
    }
#endif
  } else {
    applyCpuFrequency(CPU_FREQ_LOW);
    networkManager.setWifiPowerSaving(true);

#if LV_VERSION_MAJOR >= 9
    if (disp) {
        lv_timer_t * rt = lv_display_get_refr_timer(disp);
        if(rt) lv_timer_set_period(rt, 1000 / FPS_LOW);
    }
#endif
  }
}

uint32_t PowerManager::getSleepTimeout() const {
  const DeviceConfig& cfg = configManager.getConfig();
  if (isPoweredByMains()) {
    if (!cfg.auto_sleep_enabled) return 0xFFFFFFFF;
    return cfg.auto_sleep_seconds * 1000UL;
  }
  if (!cfg.auto_sleep_battery_enabled) return 0xFFFFFFFF;
  return cfg.auto_sleep_battery_seconds * 1000UL;
}

bool PowerManager::isPoweredByMains() const {
  return (M5.Power.getBatteryCurrent() <= 50);
}

void PowerManager::update(uint32_t last_activity_time) {
  if (is_display_sleeping) return;
  uint32_t now = millis();

  // Schlaf-Sperre aktiv? -> Keine Sleep-Checks, optional Timeout pruefen
  if (sleep_blocked) {
    if (sleep_block_until != 0 && (int32_t)(now - sleep_block_until) >= 0) {
      sleep_blocked = false;  // Sperre abgelaufen
    } else {
      // Sicherstellen, dass wir nicht in den Low-Power-Sleep fallen
      setHighPerformance(true);
      return;
    }
  }

  if (is_high_performance && (now - last_activity_time > IDLE_TIMEOUT_MS)) {
    setHighPerformance(false); 
  }

  uint32_t sleep_timeout = getSleepTimeout();
  if (!is_high_performance && (now - last_activity_time > sleep_timeout)) {
    enterDisplaySleep();
  }
}

void PowerManager::enterDisplaySleep() {
  if (is_display_sleeping) return;
  Serial.println("😴 Sleep...");
  
  saved_brightness = M5.Display.getBrightness();
  M5.update();
  M5.Display.sleep();
  displayManager.setInputEnabled(false);
  applyCpuFrequency(CPU_FREQ_SLEEP);

#if LV_VERSION_MAJOR >= 9
    if (disp) {
        lv_timer_t * rt = lv_display_get_refr_timer(disp);
        if(rt) lv_timer_pause(rt);
    }
#endif
  
  networkManager.setWifiPowerSaving(true);
  is_display_sleeping = true;
  is_high_performance = false;
  mqttPublishDeviceSettings();
}

void PowerManager::wakeFromDisplaySleep() {
  if (!is_display_sleeping) return;

  displayManager.setInputEnabled(true);
  M5.Display.wakeup();
  M5.Display.setBrightness(saved_brightness);
  applyCpuFrequency(CPU_FREQ_HIGH);
  
#if LV_VERSION_MAJOR >= 9
    if (disp) {
        lv_timer_t * rt = lv_display_get_refr_timer(disp);
        if(rt) {
            lv_timer_resume(rt);
            lv_timer_set_period(rt, 1000 / FPS_HIGH);
        }
    }
#endif
  
  is_display_sleeping = false;
  is_high_performance = true;
  
  if (isPoweredByMains()) networkManager.setWifiPowerSaving(false);
  displayManager.resetActivityTimer();
  displayManager.armWakeTouchGuard();
  mqttPublishDeviceSettings();
}

void PowerManager::updatePowerMode() {
}

void PowerManager::blockSleep(uint32_t duration_ms) {
  sleep_blocked = true;
  sleep_block_until = duration_ms ? millis() + duration_ms : 0;
  if (is_display_sleeping) {
    wakeFromDisplaySleep();
  }
  setHighPerformance(true);
}

void PowerManager::allowSleep() {
  sleep_blocked = false;
  sleep_block_until = 0;
}

bool PowerManager::isSleepBlocked() const {
  if (!sleep_blocked) return false;
  if (sleep_block_until == 0) return true;
  return (int32_t)(millis() - sleep_block_until) < 0;
}

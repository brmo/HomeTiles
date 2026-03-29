#include "src/devices/m5stack_tab5/device_m5stack_tab5.h"

#if defined(DEVICE_M5STACK_TAB5) || defined(DEVICE_TAB5)

#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>

namespace {

constexpr int kSdSckPin = 43;
constexpr int kSdMisoPin = 39;
constexpr int kSdMosiPin = 44;
constexpr int kSdCsPin = 42;
constexpr uint32_t kSdClockHz = 25000000;

bool g_m5_ready = false;
bool g_sd_init_attempted = false;
bool g_sd_available = false;
uint32_t g_sd_retry_tick_ms = 0;
uint8_t g_brightness = 150;

uint8_t to_m5_rotation(uint8_t logical_rotation) {
  logical_rotation &= 0x03;
  return (logical_rotation & 0x02) ? 3 : 1;
}

}  // namespace

bool DeviceM5StackTab5::init() {
  if (g_m5_ready) {
    return true;
  }

  Serial.println("[Device/M5StackTab5] Initialising board...");

  auto cfg = M5.config();
  M5.begin(cfg);
  Wire.setClock(400000);

  M5.Display.setRotation(to_m5_rotation(kProfile.rotation_default));
  M5.Display.fillScreen(0x0000);
  M5.Display.setBrightness(g_brightness);

  g_m5_ready = true;
  Serial.println("[Device/M5StackTab5] Display OK");
  return true;
}

void DeviceM5StackTab5::displayPushPixels(int32_t x, int32_t y, int32_t w, int32_t h,
                                          const uint16_t* data) {
  if (!g_m5_ready || !data) {
    return;
  }

  M5.Display.pushImage(x, y, w, h, const_cast<uint16_t*>(data));
}

void DeviceM5StackTab5::displayPushPixelsDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                                             const uint16_t* data) {
  if (!g_m5_ready || !data) {
    return;
  }

  M5.Display.pushImageDMA(x, y, w, h, const_cast<uint16_t*>(data));
}

void DeviceM5StackTab5::displayWaitDMA() {
  if (!g_m5_ready) {
    return;
  }

  M5.Display.waitDMA();
}

void DeviceM5StackTab5::displayFillScreen(uint16_t color) {
  if (!g_m5_ready) {
    return;
  }

  M5.Display.fillScreen(color);
}

void DeviceM5StackTab5::displaySetRotation(uint8_t rotation) {
  if (!g_m5_ready) {
    return;
  }

  M5.Display.setRotation(to_m5_rotation(rotation));
}

void DeviceM5StackTab5::setBrightness(uint8_t value) {
  g_brightness = value;
  if (!g_m5_ready) {
    return;
  }

  M5.Display.setBrightness(value);
}

uint8_t DeviceM5StackTab5::getBrightness() {
  return g_m5_ready ? M5.Display.getBrightness() : g_brightness;
}

bool DeviceM5StackTab5::getTouch(int16_t& x, int16_t& y) {
  if (!g_m5_ready) {
    return false;
  }

  lgfx::touch_point_t tp;
  if (!M5.Display.getTouch(&tp)) {
    return false;
  }

  x = tp.x;
  y = tp.y;
  return true;
}

void DeviceM5StackTab5::displaySleep() {
  if (!g_m5_ready) {
    return;
  }

  M5.Display.setBrightness(0);
  M5.Display.waitDisplay();
  M5.Display.sleep();
}

void DeviceM5StackTab5::displayWake() {
  if (!g_m5_ready) {
    return;
  }

  M5.Display.wakeup();
  M5.Display.powerSaveOff();
  M5.Display.waitDisplay();
  M5.Display.setBrightness(g_brightness);
}

void DeviceM5StackTab5::displayPowerSaveOn() {
  if (!g_m5_ready) {
    return;
  }

  M5.Display.powerSaveOn();
}

void DeviceM5StackTab5::displayPowerSaveOff() {
  if (!g_m5_ready) {
    return;
  }

  M5.Display.powerSaveOff();
}

void DeviceM5StackTab5::displayWaitDisplay() {
  if (!g_m5_ready) {
    return;
  }

  M5.Display.waitDisplay();
}

bool DeviceM5StackTab5::initSDCard() {
  const uint8_t card_type = SD.cardType();
  if (g_sd_available && card_type != CARD_NONE) {
    return true;
  }

  const uint32_t now = millis();
  if (g_sd_init_attempted && (now - g_sd_retry_tick_ms) < 1500) {
    return false;
  }
  g_sd_init_attempted = true;
  g_sd_retry_tick_ms = now;

  SPI.begin(kSdSckPin, kSdMisoPin, kSdMosiPin, kSdCsPin);

  if (!SD.begin(kSdCsPin, SPI, kSdClockHz)) {
    g_sd_available = false;
    Serial.println("[Device/M5StackTab5] SD card mount failed");
    return false;
  }

  if (SD.cardType() == CARD_NONE) {
    g_sd_available = false;
    Serial.println("[Device/M5StackTab5] SD card absent after mount");
    return false;
  }

  g_sd_available = true;
  Serial.printf("[Device/M5StackTab5] SD card OK, type=%u, size=%llu MB\n",
                static_cast<unsigned>(SD.cardType()),
                static_cast<unsigned long long>(SD.cardSize() / (1024ULL * 1024ULL)));
  return true;
}

bool DeviceM5StackTab5::storageReady() {
  return initSDCard();
}

fs::FS& DeviceM5StackTab5::storageFS() {
  return SD;
}

#endif

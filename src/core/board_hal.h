#ifndef BOARD_HAL_H
#define BOARD_HAL_H

#include <Arduino.h>
#include <driver/gpio.h>

// Board HAL - Hardware Abstraction Layer
// Waveshare ESP32-P4-WIFI6-Touch-LCD-4B (720x720 MIPI DSI)
// Uses the official Waveshare Arduino display/touch stack.

// ---------- Pin Definitions ----------
#define BSP_LCD_BACKLIGHT   GPIO_NUM_26
#define BSP_LCD_RST         GPIO_NUM_27
#define BSP_I2C_SDA         GPIO_NUM_7
#define BSP_I2C_SCL         GPIO_NUM_8
#define BSP_LCD_TOUCH_RST   GPIO_NUM_23
#define BSP_LCD_TOUCH_INT   GPIO_NUM_NC

// SD Card (SDMMC, 4-bit)
#define BSP_SD_CLK          GPIO_NUM_43
#define BSP_SD_CMD          GPIO_NUM_44
#define BSP_SD_D0           GPIO_NUM_39
#define BSP_SD_D1           GPIO_NUM_40
#define BSP_SD_D2           GPIO_NUM_41
#define BSP_SD_D3           GPIO_NUM_42

// I2S Audio (unused by this project for now)
#define BSP_I2S_MCLK        GPIO_NUM_13
#define BSP_I2S_SCLK        GPIO_NUM_12
#define BSP_I2S_LCLK        GPIO_NUM_10
#define BSP_I2S_DOUT        GPIO_NUM_9
#define BSP_I2S_DSIN        GPIO_NUM_11
#define BSP_POWER_AMP_IO    GPIO_NUM_53

// Display
#define BSP_LCD_H_RES       720
#define BSP_LCD_V_RES       720

// Backlight PWM
#define BSP_BL_LEDC_CHANNEL 0
#define BSP_BL_LEDC_TIMER   1
#define BSP_BL_LEDC_FREQ    5000
#define BSP_BL_LEDC_BITS    10

namespace BoardHAL {

bool init();

void displayPushPixels(int32_t x, int32_t y, int32_t w, int32_t h,
                       const uint16_t* data);
void displayPushPixelsDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                          const uint16_t* data);
void displayWaitDMA();
void displayFillScreen(uint16_t color);

void setBrightness(uint8_t value);
uint8_t getBrightness();

struct TouchPoint {
  int16_t x;
  int16_t y;
};

bool getTouch(TouchPoint* tp);

void displaySleep();
void displayWake();
void displayPowerSaveOn();
void displayPowerSaveOff();
void displayWaitDisplay();

bool initSDCard();

}  // namespace BoardHAL

#endif // BOARD_HAL_H

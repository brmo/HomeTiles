#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* esp_lcd_touch_handle_t;

#define ESP_LCD_TOUCH_MAX_POINTS 5

esp_lcd_touch_handle_t touch_gsl3680_init(DEV_I2C_Port* i2c_port);
bool esp_lcd_touch_read_data(esp_lcd_touch_handle_t handle);
bool esp_lcd_touch_get_coordinates(esp_lcd_touch_handle_t handle,
                                   uint16_t* x, uint16_t* y,
                                   uint16_t* strength, uint8_t* count,
                                   uint8_t max_points);

#ifdef __cplusplus
}
#endif

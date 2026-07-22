#pragma once

#include <stdint.h>
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXAMPLE_I2C_MASTER_SDA GPIO_NUM_7
#define EXAMPLE_I2C_MASTER_SCL GPIO_NUM_8
#define EXAMPLE_I2C_MASTER_FREQUENCY (400 * 1000)
#define EXAMPLE_I2C_MASTER_NUM I2C_NUM_0

typedef struct {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t dev;
} DEV_I2C_Port;

DEV_I2C_Port DEV_I2C_Init(void);

#ifdef __cplusplus
}
#endif

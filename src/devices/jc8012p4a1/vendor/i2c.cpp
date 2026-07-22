#include "i2c.h"
#include <driver/i2c_master.h>
#include "esp_log.h"

DEV_I2C_Port DEV_I2C_Init(void) {
    DEV_I2C_Port port = {};

    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = EXAMPLE_I2C_MASTER_NUM;
    bus_config.sda_io_num = EXAMPLE_I2C_MASTER_SDA;
    bus_config.scl_io_num = EXAMPLE_I2C_MASTER_SCL;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_config, &port.bus);
    if (err != ESP_OK) {
        ESP_LOGE("I2C", "Failed to create I2C master bus: %d", err);
        return {};
    }

    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = 0x40;
    dev_config.scl_speed_hz = EXAMPLE_I2C_MASTER_FREQUENCY;

    err = i2c_master_bus_add_device(port.bus, &dev_config, &port.dev);
    if (err != ESP_OK) {
        ESP_LOGE("I2C", "Failed to add I2C device: %d", err);
        return {};
    }

    return port;
}

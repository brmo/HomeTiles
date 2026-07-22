#include "gsl3680_touch.h"
#include "gsl3680_firmware.h"
#include "gsl_point_id.h"
#include <driver/i2c_master.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include <string.h>

static const char* TAG = "gsl3680";

// Config data from the BSP (profi-max/JC8012P4A1_BSP_ESP32P4).
// Identical to gsl_config_data_id in esp_lcd_gsl3680.c.
static unsigned int gsl_config_data_id[] = {
    0xccb69a,
    0x200,
    0,0,
    0,
    0,0,0,
    0,0,0,0,0,0,0,0x1cc86fd6,

    0x40000d00,0xa,0xe001a,0xe001a,0x3200500,0,0x5100,0x8e00,
    0,0x320014,0,0x14,0,0,0,0,
    0x8,0x4000,0x1000,0x10170002,0x10110000,0,0,0x4040404,
    0x1b6db688,0x64,0xb3000f,0xad0019,0xa60023,0xa0002d,0xb3000f,0xad0019,
    0xa60023,0xa0002d,0xb3000f,0xad0019,0xa60023,0xa0002d,0xb3000f,0xad0019,
    0xa60023,0xa0002d,0x804000,0x90040,0x90001,0,0,0,
    0,0,0,0x14012c,0xa003c,0xa0078,0x400,0x1081,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,

    0,
    0x3200384,0x64,0x503e8,
    0,0,0,
    0,0,0,
    0,0,0,
    0,0,0,
    0,0,0,
    0,0,0,
    0,0,0,
    0,0,0,

    0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,

    0x220,
    0,0,0,0,0,0,0,0,
    0x10203,0x4050607,0x8090a0b,0xc0d0e0f,0x10111213,0x14151617,0x18191a1b,0x1c1d1e1f,
    0x20212223,0x24252627,0x28292a2b,0x2c2d2e2f,0x30313233,0x34353637,0x38393a3b,0x3c3d3e3f,
    0x10203,0x4050607,0x8090a0b,0xc0d0e0f,0x10111213,0x14151617,0x18191a1b,0x1c1d1e1f,
    0x20212223,0x24252627,0x28292a2b,0x2c2d2e2f,0x30313233,0x34353637,0x38393a3b,0x3c3d3e3f,

    0x10203,0x4050607,0x8090a0b,0xc0d0e0f,0x10111213,0x14151617,0x18191a1b,0x1c1d1e1f,
    0x20212223,0x24252627,0x28292a2b,0x2c2d2e2f,0x30313233,0x34353637,0x38393a3b,0x3c3d3e3f,

    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,

    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,

    0x10203,0x4050607,0x8090a0b,0xc0d0e0f,0x10111213,0x14151617,0x18191a1b,0x1c1d1e1f,
    0x20212223,0x24252627,0x28292a2b,0x2c2d2e2f,0x30313233,0x34353637,0x38393a3b,0x3c3d3e3f,

    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,

    0x3,
    0x101,0,0x100,0,
    0x20,0x10,0x8,0x4,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,

    0x4,0,0,0,0,0,0,0,
    0x3800680,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,
};
#define GSL3680_I2C_ADDR 0x40
#define GSL3680_MAX_CONTACTS 5
#define TOUCH_RESET_PIN 22

typedef struct {
    DEV_I2C_Port* i2c;
    bool initialized;
    uint16_t last_x[ESP_LCD_TOUCH_MAX_POINTS];
    uint16_t last_y[ESP_LCD_TOUCH_MAX_POINTS];
    uint16_t last_strength[ESP_LCD_TOUCH_MAX_POINTS];
    uint8_t last_count;
} gsl3680_state_t;

static gsl3680_state_t s_state = {};

static esp_err_t gsl_write_reg(uint8_t reg, const uint8_t* data, size_t len) {
    uint8_t buf[5];
    buf[0] = reg;
    for (size_t i = 0; i < len && i < 4; i++) buf[i + 1] = data[i];
    return i2c_master_transmit(s_state.i2c->dev, buf, 1 + len, 100);
}

static esp_err_t gsl_read_reg(uint8_t reg, uint8_t* data, size_t len) {
    return i2c_master_transmit_receive(s_state.i2c->dev, &reg, 1, data, len, 100);
}

static void gsl_reset_pin(int pin, bool high) {
    gpio_set_level(static_cast<gpio_num_t>(pin), high ? 1 : 0);
}

static esp_err_t gsl_load_firmware(void) {
    uint16_t count = sizeof(GSLX680_FW) / sizeof(struct gsl3680_fw_data);
    uint8_t buf[5];
    for (uint16_t i = 0; i < count; i++) {
        buf[0] = (uint8_t)(GSLX680_FW[i].val & 0xff);
        buf[1] = (uint8_t)((GSLX680_FW[i].val >> 8) & 0xff);
        buf[2] = (uint8_t)((GSLX680_FW[i].val >> 16) & 0xff);
        buf[3] = (uint8_t)((GSLX680_FW[i].val >> 24) & 0xff);
        size_t wlen = (GSLX680_FW[i].offset == 0xf0) ? 1 : 4;
        esp_err_t err = gsl_write_reg(GSLX680_FW[i].offset, buf, wlen);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "FW write fail at %u: %d", i, err);
            return err;
        }
    }
    return ESP_OK;
}

static esp_err_t gsl_do_init_sequence(void) {
    uint8_t val;

    gsl_reset_pin(TOUCH_RESET_PIN, false);
    ets_delay_us(20000);
    gsl_reset_pin(TOUCH_RESET_PIN, true);
    ets_delay_us(20000);

    val = 0x88;
    gsl_write_reg(0xe0, &val, 1);
    ets_delay_us(10000);
    val = 0x04;
    gsl_write_reg(0xe4, &val, 1);
    ets_delay_us(10000);
    uint8_t zeros[4] = {0, 0, 0, 0};
    gsl_write_reg(0xbc, zeros, 4);
    ets_delay_us(10000);

    uint8_t cfg[4];
    uint8_t probe[4] = {0x12, 0x34, 0x56, 0x00};
    ets_delay_us(50000);
    gsl_read_reg(0xf0, cfg, 4);
    ets_delay_us(20000);
    gsl_write_reg(0xf0, probe, 4);
    ets_delay_us(20000);
    gsl_read_reg(0xf0, cfg, 4);
    ets_delay_us(20000);
    if (cfg[0] != 0x12) {
        ESP_LOGE(TAG, "Config probe failed: 0x%02x", cfg[0]);
        return ESP_FAIL;
    }

    uint8_t clr_regs[4] = {0xe0, 0x80, 0xe4, 0xe0};
    uint8_t clr_data[4] = {0x88, GSL3680_MAX_CONTACTS, 0x04, 0x00};
    for (int i = 0; i < 4; i++) {
        gsl_write_reg(clr_regs[i], &clr_data[i], 1);
        ets_delay_us(20000);
    }

    gsl_reset_pin(TOUCH_RESET_PIN, false);
    ets_delay_us(20000);
    gsl_reset_pin(TOUCH_RESET_PIN, true);
    ets_delay_us(20000);

    val = 0x88;
    gsl_write_reg(0xe0, &val, 1);
    ets_delay_us(10000);
    val = 0x04;
    gsl_write_reg(0xe4, &val, 1);
    ets_delay_us(10000);
    gsl_write_reg(0xbc, zeros, 4);
    ets_delay_us(10000);

    esp_err_t err = gsl_load_firmware();
    if (err != ESP_OK) return err;

    val = 0x00;
    err = gsl_write_reg(0xe0, &val, 1);
    if (err != ESP_OK) return err;
    ets_delay_us(10000);

    uint8_t ram[4];
    ets_delay_us(30000);
    err = gsl_read_reg(0xb0, ram, 4);
    if (err != ESP_OK) return err;
    for (int i = 0; i < 4; i++) {
        if (ram[i] != 0x5a) {
            ESP_LOGE(TAG, "RAM verify fail: %02x %02x %02x %02x", ram[0], ram[1], ram[2], ram[3]);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_lcd_touch_handle_t touch_gsl3680_init(DEV_I2C_Port* i2c_port) {
    if (!i2c_port) return NULL;

    s_state.i2c = i2c_port;
    s_state.initialized = false;
    s_state.last_count = 0;

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << TOUCH_RESET_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    gsl_DataInit(gsl_config_data_id);

    for (int attempt = 0; attempt < 3; attempt++) {
        if (gsl_do_init_sequence() == ESP_OK) {
            s_state.initialized = true;
            ESP_LOGI(TAG, "GSL3680 initialized (attempt %d)", attempt + 1);
            return &s_state;
        }
        ESP_LOGW(TAG, "Init attempt %d failed, power cycling", attempt + 1);
        gsl_reset_pin(TOUCH_RESET_PIN, false);
        ets_delay_us(50000);
        gsl_reset_pin(TOUCH_RESET_PIN, true);
        ets_delay_us(30000);
        gsl_reset_pin(TOUCH_RESET_PIN, false);
        ets_delay_us(5000);
        gsl_reset_pin(TOUCH_RESET_PIN, true);
        ets_delay_us(20000);
    }

    ESP_LOGE(TAG, "GSL3680 init failed after 3 attempts");
    return NULL;
}

bool esp_lcd_touch_read_data(esp_lcd_touch_handle_t handle) {
    if (!handle || !s_state.initialized) return false;

    gsl3680_state_t* state = (gsl3680_state_t*)handle;
    uint8_t touch_data[24];

    esp_err_t err = gsl_read_reg(0x80, touch_data, 24);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Touch read failed: %d", err);
        return false;
    }

    uint8_t raw_count = touch_data[0] & 0x0f;
    if (raw_count > 2) raw_count = 2;

    struct gsl_touch_info cinfo = {};
    cinfo.finger_num = raw_count;

    if (raw_count >= 1) {
        cinfo.x[0] = ((touch_data[7] & 0x0f) << 8) | touch_data[6];
        cinfo.y[0] = (touch_data[5] << 8) | touch_data[4];
        cinfo.id[0] = (touch_data[7] & 0xf0) >> 4;
    }
    if (raw_count >= 2) {
        cinfo.x[1] = ((touch_data[11] & 0x0f) << 8) | touch_data[10];
        cinfo.y[1] = (touch_data[9] << 8) | touch_data[8];
        cinfo.id[1] = (touch_data[11] & 0xf0) >> 4;
    }

    gsl_alg_id_main(&cinfo);

    state->last_count = cinfo.finger_num;
    if (cinfo.finger_num >= 1) {
        state->last_x[0] = static_cast<uint16_t>(cinfo.x[0]);
        state->last_y[0] = static_cast<uint16_t>(cinfo.y[0]);
        state->last_strength[0] = 100;
    }
    if (cinfo.finger_num >= 2) {
        state->last_x[1] = static_cast<uint16_t>(cinfo.x[1]);
        state->last_y[1] = static_cast<uint16_t>(cinfo.y[1]);
        state->last_strength[1] = 100;
    }

    return true;
}

bool esp_lcd_touch_get_coordinates(esp_lcd_touch_handle_t handle,
                                   uint16_t* x, uint16_t* y,
                                   uint16_t* strength, uint8_t* count,
                                   uint8_t max_points) {
    if (!handle || !x || !y || !strength || !count) return false;

    gsl3680_state_t* state = (gsl3680_state_t*)handle;
    uint8_t n = state->last_count;
    if (n > max_points) n = max_points;

    for (uint8_t i = 0; i < n; i++) {
        x[i] = state->last_x[i];
        y[i] = state->last_y[i];
        strength[i] = state->last_strength[i];
    }
    for (uint8_t i = n; i < max_points; i++) {
        x[i] = 0;
        y[i] = 0;
        strength[i] = 0;
    }

    *count = n;
    return (n > 0);
}

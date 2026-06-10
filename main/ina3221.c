#include "ina3221.h"
#include "gpio_config.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "ina3221";

// Registres INA3221
#define REG_CONFIG      0x00
#define REG_SHUNT_CH1   0x01   // Shunt voltage CH1 — 40μV/LSB (bits [15:3])
#define REG_BUS_CH1     0x02   // Bus voltage CH1  —  8mV/LSB  (bits [15:3])
// CH2 : 0x03 (shunt), 0x04 (bus)
// CH3 : 0x05 (shunt), 0x06 (bus)

#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_FREQ_HZ  400000
#define I2C_TIMEOUT_MS      100

#define SHUNT_MOHM          100    // 0.1Ω = 100mΩ
#define SHUNT_UV_PER_LSB    40.0f  // 40μV par LSB
#define BUS_MV_PER_LSB      8.0f   // 8mV par LSB

static bool s_ok = false;

// ── I2C helpers ───────────────────────────────────────────────────────────────

static esp_err_t i2c_write_reg(uint8_t reg, uint16_t val)
{
    uint8_t buf[3] = {reg, (val >> 8) & 0xFF, val & 0xFF};
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA3221_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, buf, 3, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t i2c_read_reg(uint8_t reg, uint16_t *val)
{
    uint8_t buf[2] = {0, 0};

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA3221_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA3221_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &buf[0], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &buf[1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK)
        *val = ((uint16_t)buf[0] << 8) | buf[1];

    return ret;
}

// ── Init ──────────────────────────────────────────────────────────────────────

esp_err_t ina3221_init(void)
{
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_SDA_PIN,
        .scl_io_num       = I2C_SCL_PIN,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install: %s", esp_err_to_name(ret));
        return ret;
    }

    // Config: CH1+CH2+CH3 activés, mode continu, 1 sample, 1.1ms conversion
    // 0x7127 = valeur par défaut INA3221 (tous canaux ON, continu)
    ret = i2c_write_reg(REG_CONFIG, 0x7127);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Écriture config échouée — INA3221 absent ou mauvaise adresse");
        return ret;
    }

    s_ok = true;
    ESP_LOGI(TAG, "init OK — SDA=%d SCL=%d addr=0x%02X", I2C_SDA_PIN, I2C_SCL_PIN, INA3221_I2C_ADDR);
    return ESP_OK;
}

// ── Lecture ───────────────────────────────────────────────────────────────────

float ina3221_lire_tension(int canal)
{
    if (!s_ok) return 0.0f;

    // Registre bus voltage : CH1=0x02, CH2=0x04, CH3=0x06
    uint8_t reg = (uint8_t)(REG_BUS_CH1 + (canal - 1) * 2);
    uint16_t raw = 0;
    if (i2c_read_reg(reg, &raw) != ESP_OK) return 0.0f;

    // Bits [15:3], non signé, LSB = 8mV
    return (float)(raw >> 3) * (BUS_MV_PER_LSB / 1000.0f);
}

ina3221_mesure_t ina3221_lire_canal(int canal)
{
    ina3221_mesure_t m = {0.0f, 0.0f};
    if (!s_ok) return m;

    uint8_t reg_shunt = (uint8_t)(REG_SHUNT_CH1 + (canal - 1) * 2);
    uint8_t reg_bus   = (uint8_t)(REG_BUS_CH1   + (canal - 1) * 2);

    uint16_t raw_shunt = 0, raw_bus = 0;
    if (i2c_read_reg(reg_shunt, &raw_shunt) != ESP_OK) return m;
    if (i2c_read_reg(reg_bus,   &raw_bus)   != ESP_OK) return m;

    // Bus voltage : bits [15:3], non signé, LSB = 8mV
    m.tension_v = (float)(raw_bus >> 3) * (BUS_MV_PER_LSB / 1000.0f);

    // Shunt voltage : bits [15:3], signé, LSB = 40μV → courant en mA
    int16_t shunt_raw = (int16_t)raw_shunt >> 3;
    float shunt_uv = (float)shunt_raw * SHUNT_UV_PER_LSB;
    m.courant_ma = shunt_uv / (float)SHUNT_MOHM;   // uV / mΩ = mA

    return m;
}

bool ina3221_est_ok(void)
{
    return s_ok;
}

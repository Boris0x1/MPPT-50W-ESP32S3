#include "ina226_bus.h"
#include "mppt_config.h"
#include "driver/i2c.h"

// La API de comandos de ESP-IDF 6.1 admite una lista mayor que los ocho
// comandos hardware. El driver la alimenta por interrupciones. No mezclar
// con driver/i2c_master.h en este firmware. Migrar este modulo al subir a IDF 7.
_Alignas(4) static uint8_t command_buffer[I2C_LINK_RECOMMENDED_SIZE(6)];

esp_err_t ina226_bus_init(void)
{
    const i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = INA226_SDA_GPIO, .scl_io_num = INA226_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = INA226_I2C_SPEED_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_NUM_0, &config);
    return err == ESP_OK ? i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0) : err;
}

esp_err_t ina226_bus_write(uint8_t reg, uint16_t value)
{
    const uint8_t data[] = {reg, (uint8_t)(value >> 8), (uint8_t)value};
    return i2c_master_write_to_device(I2C_NUM_0, INA226_I2C_ADDRESS, data, sizeof(data),
                                      pdMS_TO_TICKS(INA226_I2C_TIMEOUT_MS));
}

esp_err_t ina226_bus_read(uint8_t reg, uint16_t *value)
{
    uint8_t data[2];
    const esp_err_t err = i2c_master_write_read_device(I2C_NUM_0, INA226_I2C_ADDRESS,
        &reg, 1, data, sizeof(data), pdMS_TO_TICKS(INA226_I2C_TIMEOUT_MS));
    if (err == ESP_OK) *value = ((uint16_t)data[0] << 8) | data[1];
    return err;
}

esp_err_t ina226_bus_read_frame(uint16_t *status, uint16_t *shunt, uint16_t *bus)
{
    const uint8_t registers[] = {0x06, 0x01, 0x02};
    uint8_t data[6];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(command_buffer, sizeof(command_buffer));
    if (!cmd) return ESP_ERR_NO_MEM;
    esp_err_t err = ESP_OK;
#define APPEND(operation) do { err = (operation); if (err != ESP_OK) goto finish; } while (0)
    for (unsigned i = 0; i < 3; ++i) {
        // Cada registro se direcciona explicitamente; el INA226 NO autoincrementa.
        APPEND(i2c_master_start(cmd));
        APPEND(i2c_master_write_byte(cmd, INA226_I2C_ADDRESS << 1, true));
        APPEND(i2c_master_write_byte(cmd, registers[i], true));
        APPEND(i2c_master_start(cmd));
        APPEND(i2c_master_write_byte(cmd, (INA226_I2C_ADDRESS << 1) | 1U, true));
        APPEND(i2c_master_read(cmd, &data[2 * i], 2, I2C_MASTER_LAST_NACK));
    }
    APPEND(i2c_master_stop(cmd));
    // Una sola entrada al driver y una sola espera de finalizacion, a 400 kHz.
    err = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(INA226_I2C_TIMEOUT_MS));
    if (err == ESP_OK) {
        *status = ((uint16_t)data[0] << 8) | data[1];
        *shunt = ((uint16_t)data[2] << 8) | data[3];
        *bus = ((uint16_t)data[4] << 8) | data[5];
    }
finish:
    i2c_cmd_link_delete_static(cmd);
    return err;
#undef APPEND
}

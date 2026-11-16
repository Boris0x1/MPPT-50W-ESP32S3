#pragma once

#include <stdint.h>
#include "esp_err.h"

// Solo la tarea de adquisicion usa este bus y su buffer estatico.
esp_err_t ina226_bus_init(void);
esp_err_t ina226_bus_write(uint8_t reg, uint16_t value);
esp_err_t ina226_bus_read(uint8_t reg, uint16_t *value);
esp_err_t ina226_bus_read_frame(uint16_t *status, uint16_t *shunt, uint16_t *bus);

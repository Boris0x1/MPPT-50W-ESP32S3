#pragma once
#include <stdint.h>
#include "esp_err.h"

// Una sola tarea escribe el PWM despues de la inicializacion.
esp_err_t mosfet_pwm_init(void);
// Ticks nativos MCPWM. Cero fuerza LOW inmediatamente y detiene el contador.
esp_err_t mosfet_pwm_set_count(uint32_t compare_ticks);

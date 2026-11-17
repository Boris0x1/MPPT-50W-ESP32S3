// Adaptado de WEB_MPPT_ESP32-C6/main/mppt_loads.c para los dos bancos del S3.
#include "mppt_loads.h"
#include "mppt_config.h"
#include "driver/gpio.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#define LOAD_BIT64(pin) (UINT64_C(1) << (pin))
#define LOAD_MASK (LOAD_BIT64(LOAD_GPIO_30) | LOAD_BIT64(LOAD_GPIO_10) | LOAD_BIT64(LOAD_GPIO_20))
#define ACTIVE_LOW_MASK LOAD_BIT64(LOAD_GPIO_10)
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static mppt_loads_state_t g_load;

static int gpio_for_load(int ohms)
{
    switch (ohms) {
    case 10: return LOAD_GPIO_10;
    case 20: return LOAD_GPIO_20;
    case 30: return LOAD_GPIO_30;
    default: return -1;
    }
}

static uint64_t output_levels(void)
{
    return (uint64_t)GPIO.out | ((uint64_t)GPIO.out1.val << 32);
}

static void snapshot_locked(mppt_loads_state_t *out)
{
    *out = g_load;
    const uint64_t enabled = (uint64_t)GPIO.enable | ((uint64_t)GPIO.enable1.val << 32);
    const uint64_t expected = LOAD_BIT64(g_load.gpio) ^ ACTIVE_LOW_MASK;
    out->active = g_load.ready && (output_levels() & LOAD_MASK) == expected &&
                  (enabled & LOAD_MASK) == LOAD_MASK;
}

static void write_pattern_locked(int gpio)
{
    g_load.switched_before_us = (uint64_t)esp_timer_get_time();
    // W1TS/W1TC por pin: no leer-modificar-escribir bancos completos, ya que
    // CPU0/RMT/UART usan otros GPIO del mismo banco. Primero desactivar todas
    // las ramas, luego activar la elegida. Sin esperas dentro del lazo MPPT.
    gpio_ll_set_level(&GPIO, LOAD_GPIO_30, 0);
    gpio_ll_set_level(&GPIO, LOAD_GPIO_10, 1);
    gpio_ll_set_level(&GPIO, LOAD_GPIO_20, 0);
    gpio_ll_set_level(&GPIO, gpio, gpio == LOAD_GPIO_10 ? 0 : 1);
    g_load.switched_after_us = (uint64_t)esp_timer_get_time();
}

esp_err_t mppt_loads_init(void)
{
    portENTER_CRITICAL(&g_mux);
    const bool ready = g_load.ready;
    portEXIT_CRITICAL(&g_mux);
    if (ready) return ESP_OK;
    // Latches inactivos antes de habilitar salidas, con PWM todavia en cero.
    esp_err_t err = gpio_set_level(LOAD_GPIO_30, 0);
    if (err == ESP_OK) err = gpio_set_level(LOAD_GPIO_10, 1);
    if (err == ESP_OK) err = gpio_set_level(LOAD_GPIO_20, 0);
    if (err != ESP_OK) return err;
    const gpio_config_t active_low = {.pin_bit_mask = ACTIVE_LOW_MASK, .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    const gpio_config_t active_high = {.pin_bit_mask = LOAD_MASK & ~ACTIVE_LOW_MASK, .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE};
    err = gpio_config(&active_low);
    if (err == ESP_OK) err = gpio_config(&active_high);
    if (err != ESP_OK) return err;
    portENTER_CRITICAL(&g_mux);
    write_pattern_locked(gpio_for_load(LOAD_INITIAL_OHMS));
    g_load.ready = true; g_load.ohms = LOAD_INITIAL_OHMS;
    g_load.gpio = gpio_for_load(LOAD_INITIAL_OHMS); g_load.revision = 1;
    mppt_loads_state_t state; snapshot_locked(&state);
    portEXIT_CRITICAL(&g_mux);
    return state.active ? ESP_OK : ESP_FAIL;
}

esp_err_t mppt_loads_select(int ohms, mppt_loads_state_t *out)
{
    const int gpio = gpio_for_load(ohms);
    if (gpio < 0) { mppt_loads_get_state(out); return ESP_ERR_INVALID_ARG; }
    portENTER_CRITICAL(&g_mux);
    mppt_loads_state_t state; snapshot_locked(&state);
    esp_err_t err = ESP_ERR_INVALID_STATE;
    if (g_load.ready) {
        if (g_load.ohms != ohms || !state.active) {
            write_pattern_locked(gpio);
            g_load.ohms = ohms; g_load.gpio = gpio;
            if (++g_load.revision == 0) g_load.revision = 1;
        }
        snapshot_locked(&state);
        err = state.active ? ESP_OK : ESP_FAIL;
    }
    if (out) *out = state;
    portEXIT_CRITICAL(&g_mux);
    return err;
}

void mppt_loads_get_state(mppt_loads_state_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&g_mux);
    snapshot_locked(out);
    portEXIT_CRITICAL(&g_mux);
}

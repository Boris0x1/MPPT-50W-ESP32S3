// Adaptado del proyecto MPPT_ESP32_H2: Modbus 01/03/0000 y DS18B20.
// El S3 mantiene cuatro buses RMT abiertos para evitar recrearlos en cada lectura.
#include "environment_sensors.h"
#include "mppt_config.h"
#include "driver/uart.h"
#include "onewire_bus.h"
#include "ds18b20.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

#define IRR_UART UART_NUM_1
#define TEMP_RETRY_US 5000000ULL
#define TEMP_ATTEMPTS 3
static const char *TAG = "AMBIENTE";
static environment_state_t g_state;
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static const int g_pins[4] = {TEMPERATURE_T0_GPIO, TEMPERATURE_T1_GPIO, TEMPERATURE_T2_GPIO, TEMPERATURE_T3_GPIO};

typedef struct {
    onewire_bus_handle_t bus;
    ds18b20_device_handle_t device;
    onewire_device_address_t address;
    uint64_t last_search_us;
    unsigned presence_failures, crc_failures;
    int last_error;
    bool searched;
} temperature_slot_t;

environment_snapshot_t environment_sensors_snapshot(uint64_t now)
{
    environment_state_t state;
    portENTER_CRITICAL(&g_mux);
    state = g_state;
    portEXIT_CRITICAL(&g_mux);
    return environment_snapshot(&state, now);
}

static esp_err_t irradiance_init(void)
{
    const uart_config_t config = {.baud_rate = IRR_UART_BAUD_RATE, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT};
    esp_err_t err = uart_driver_install(IRR_UART, 256, 256, 0, NULL, 0);
    if (err != ESP_OK) return err;
    err = uart_param_config(IRR_UART, &config);
    if (err == ESP_OK) err = uart_set_pin(IRR_UART, IRR_UART_TX_GPIO, IRR_UART_RX_GPIO,
                                         UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) uart_driver_delete(IRR_UART);
    return err;
}

static esp_err_t irradiance_read(uint16_t *value)
{
    const uint8_t request[] = {1, 3, 0, 0, 0, 1, 0x84, 0x0A};
    uint8_t response[7];
    uart_flush_input(IRR_UART);
    if (uart_write_bytes(IRR_UART, request, sizeof(request)) != sizeof(request)) return ESP_FAIL;
    esp_err_t err = uart_wait_tx_done(IRR_UART, pdMS_TO_TICKS(100));
    if (err != ESP_OK) return err;
    const int size = uart_read_bytes(IRR_UART, response, sizeof(response), pdMS_TO_TICKS(1000));
    if (size != sizeof(response)) return ESP_ERR_TIMEOUT;
    return environment_decode_irradiance(response, size, value) ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

static void irradiance_task(void *arg)
{
    (void)arg;
    bool initialized = false;
    int last_error = ESP_OK;
    while (true) {
        uint16_t value = 0;
        esp_err_t err = initialized ? ESP_OK : irradiance_init();
        if (err == ESP_OK) { initialized = true; err = irradiance_read(&value); }
        const uint64_t now = (uint64_t)esp_timer_get_time();
        environment_state_t update = {0};
        environment_store_irradiance(&update, value, err, now);
        portENTER_CRITICAL(&g_mux);
        g_state.irr_valid = update.irr_valid; g_state.irr_error = update.irr_error;
        if (update.irr_valid) { g_state.irradiance = update.irradiance; g_state.irr_acquired_us = update.irr_acquired_us; }
        portEXIT_CRITICAL(&g_mux);
        if (err != last_error) ESP_LOGW(TAG, "Irradiancia: %s", esp_err_to_name(err));
        last_error = err;
        vTaskDelay(pdMS_TO_TICKS(initialized ? ENV_PERIOD_MS : 5000));
    }
}

static bool retryable(esp_err_t err)
{
    return err == ESP_ERR_NOT_FOUND || err == ESP_ERR_TIMEOUT || err == ESP_ERR_INVALID_CRC;
}

static esp_err_t temperature_discover(temperature_slot_t *slot, int pin)
{
    slot->searched = true;
    slot->last_search_us = (uint64_t)esp_timer_get_time();
    if (!slot->bus) {
        const onewire_bus_config_t config = {.bus_gpio_num = pin, .flags.en_pull_up = false};
        const onewire_bus_rmt_config_t rmt = {.max_rx_bytes = 10};
        esp_err_t err = onewire_new_bus_rmt(&config, &rmt, &slot->bus);
        if (err != ESP_OK) return err;
    }
    onewire_device_iter_handle_t iter = NULL;
    onewire_device_t device;
    ds18b20_device_handle_t found = NULL;
    const ds18b20_config_t config = {};
    esp_err_t err = onewire_new_device_iter(slot->bus, &iter);
    if (err == ESP_OK) err = onewire_device_iter_get_next(iter, &device);
    if (err == ESP_OK) err = ds18b20_new_device_from_enumeration(&device, &config, &found);
    onewire_device_address_t address = 0;
    if (err == ESP_OK) err = ds18b20_get_device_address(found, &address);
    if (iter) onewire_del_device_iter(iter);
    if (err == ESP_OK) {
        if (slot->device) ds18b20_del_device(slot->device);
        slot->device = found; slot->address = address;
        slot->presence_failures = slot->crc_failures = 0;
        ESP_LOGI(TAG, "GPIO%d DS18B20 ROM %016llX", pin, (unsigned long long)address);
    } else if (found) ds18b20_del_device(found);
    // Un CRC o una busqueda fallida aislada no borra la ROM conocida.
    return err;
}

static esp_err_t temperature_start(temperature_slot_t *slot)
{
    uint8_t command[10] = {0x55};
    for (unsigned i = 0; i < 8; ++i) command[i + 1] = (uint8_t)(slot->address >> (8 * i));
    command[9] = 0x44;
    esp_err_t err = ESP_FAIL;
    for (unsigned attempt = 0; attempt < TEMP_ATTEMPTS; ++attempt) {
        err = onewire_bus_reset(slot->bus);
        if (err == ESP_OK) err = onewire_bus_write_bytes(slot->bus, command, sizeof(command));
        if (!retryable(err) || attempt + 1 == TEMP_ATTEMPTS) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return err;
}

static esp_err_t temperature_read(temperature_slot_t *slot, float *value)
{
    esp_err_t err = ESP_FAIL;
    for (unsigned attempt = 0; attempt < TEMP_ATTEMPTS; ++attempt) {
        *value = NAN;
        // El componente verifica CRC del scratchpad y estado de arranque.
        err = ds18b20_get_temperature(slot->device, value);
        if (!retryable(err) || attempt + 1 == TEMP_ATTEMPTS) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return err;
}

static void temperature_task(void *arg)
{
    (void)arg;
    temperature_slot_t slots[4] = {0};
    while (true) {
        bool converting[4] = {0}, any = false;
        float values[4] = {NAN, NAN, NAN, NAN};
        int errors[4];
        for (unsigned i = 0; i < 4; ++i) {
            temperature_slot_t *slot = &slots[i];
            errors[i] = slot->last_error ? slot->last_error : ESP_ERR_NOT_FOUND;
            const uint64_t now = (uint64_t)esp_timer_get_time();
            if (!slot->searched || ((!slot->device || slot->crc_failures >= 2) &&
                now - slot->last_search_us >= TEMP_RETRY_US)) errors[i] = temperature_discover(slot, g_pins[i]);
            if (slot->device) errors[i] = temperature_start(slot);
            converting[i] = slot->device && errors[i] == ESP_OK;
            any |= converting[i];
        }
        // Iniciar los cuatro sensores antes de una sola espera. VDD/pull-up externos.
        if (any) vTaskDelay(pdMS_TO_TICKS(800));
        for (unsigned i = 0; i < 4; ++i) {
            temperature_slot_t *slot = &slots[i];
            if (converting[i]) errors[i] = temperature_read(slot, &values[i]);
            if (!errors[i] && (!isfinite(values[i]) || values[i] < -55 || values[i] > 125))
                errors[i] = ESP_ERR_INVALID_RESPONSE;
            if (slot->device && (errors[i] == ESP_ERR_NOT_FOUND || errors[i] == ESP_ERR_TIMEOUT)) {
                if (++slot->presence_failures >= 2) {
                    ds18b20_del_device(slot->device); slot->device = NULL;
                    slot->last_search_us = (uint64_t)esp_timer_get_time();
                }
            } else slot->presence_failures = 0;
            if (errors[i] == ESP_ERR_INVALID_CRC) { if (slot->crc_failures < 2) ++slot->crc_failures; }
            else slot->crc_failures = 0;
            if (errors[i] != slot->last_error) ESP_LOGW(TAG, "T%u GPIO%d: %s", i, g_pins[i], esp_err_to_name(errors[i]));
            slot->last_error = errors[i];
        }
        // Preparar fuera de la seccion critica: dentro solo se copian resultados.
        environment_state_t update;
        environment_state_init(&update);
        environment_store_temperatures(&update, values, errors, (uint64_t)esp_timer_get_time());
        portENTER_CRITICAL(&g_mux);
        memcpy(g_state.temperatures, update.temperatures, sizeof(update.temperatures));
        memcpy(g_state.temp_errors, update.temp_errors, sizeof(update.temp_errors));
        g_state.temp_mask = update.temp_mask;
        if (update.temp_mask) g_state.temp_acquired_us = update.temp_acquired_us;
        portEXIT_CRITICAL(&g_mux);
        vTaskDelay(pdMS_TO_TICKS(any ? ENV_PERIOD_MS - 800 : ENV_PERIOD_MS));
    }
}

void environment_sensors_init(void)
{
    environment_state_init(&g_state);
    // El modulo informa cambios de estado y la trama incluye errores. Evitar
    // mensajes por cada reintento de un sensor ausente que saturarian UART0.
    esp_log_level_set("1-wire", ESP_LOG_NONE);
    esp_log_level_set("1-wire.rmt", ESP_LOG_NONE);
    esp_log_level_set("1-wire.device", ESP_LOG_NONE);
    esp_log_level_set("ds18b20", ESP_LOG_NONE);
    if (xTaskCreatePinnedToCore(irradiance_task, "irr_cpu0", 4096, NULL, 1, NULL, 0) != pdPASS)
        ESP_LOGE(TAG, "Sin memoria para irradiancia");
    if (xTaskCreatePinnedToCore(temperature_task, "temp_cpu0", 6144, NULL, 1, NULL, 0) != pdPASS)
        ESP_LOGE(TAG, "Sin memoria para temperatura");
}

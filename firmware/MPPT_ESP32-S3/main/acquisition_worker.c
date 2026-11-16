#include "acquisition_worker.h"
#include "mppt_config.h"
#include "mppt_control.h"
#include "driver/gpio.h"
#include "ina226_bus.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define INA226_REG_CONFIG        0x00
#define INA226_REG_SHUNT         0x01
#define INA226_REG_BUS           0x02
#define INA226_REG_MASK_ENABLE   0x06
#define INA226_REG_MANUFACTURER  0xFE
#define INA226_REG_DIE_ID        0xFF
#define INA226_MASK_CNVR         (1U << 10)
#define INA226_FLAG_CVRF         (1U << 3)
// 16 promedios, 588 us por conversion de bus/shunt, modo disparado de ambos.
// Un par completo tarda aproximadamente 18.816 ms; despues entra en reposo.
#define INA226_CONFIG_SLOW       (0x4000U | (2U << 9) | (3U << 6) | (3U << 3) | 3U)
#define INA226_CONFIG_FAST       0x4003U // AVG=1, shunt y bus a 140 us cada uno.

static const char *TAG = "INA226_CPU0";
static TaskHandle_t g_acquisition_task;
static bool g_fast_input;
static acquisition_result_t *g_timing;

static esp_err_t ina226_write(uint8_t reg, uint16_t value)
{
    return ina226_bus_write(reg, value);
}

static esp_err_t ina226_read(uint8_t reg, uint16_t *value)
{
    return ina226_bus_read(reg, value);
}

static void ina226_alert_isr(void *arg)
{
    (void)arg;
    BaseType_t task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(g_acquisition_task, &task_woken);
    if (task_woken) {
        portYIELD_FROM_ISR();
    }
}

static void input_hardware_init(void)
{
    ESP_ERROR_CHECK(ina226_bus_init());

    // ALERT es open-drain, activo en bajo; pull-up externo a 3.3 V recomendado.
    const gpio_config_t alert_config = {
        .pin_bit_mask = 1ULL << INA226_ALERT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&alert_config));
    g_acquisition_task = xTaskGetCurrentTaskHandle();
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(INA226_ALERT_GPIO, ina226_alert_isr, NULL));
}

static esp_err_t ina226_configure(void)
{
    uint16_t manufacturer = 0, die_id = 0;
    esp_err_t err = ina226_read(INA226_REG_MANUFACTURER, &manufacturer);
    if (err != ESP_OK) return err;
    err = ina226_read(INA226_REG_DIE_ID, &die_id);
    if (err != ESP_OK) return err;
    if (manufacturer != 0x5449 || (die_id & 0xFFF0U) != 0x2260) {
        ESP_LOGE(TAG, "Dispositivo inesperado: fabricante=0x%04X ID=0x%04X",
                 (unsigned)manufacturer, (unsigned)die_id);
        return ESP_ERR_NOT_SUPPORTED;
    }
    // Reconfigurar tambien permite recuperarse de un reinicio del sensor.
    const uint16_t config = g_fast_input ? INA226_CONFIG_FAST : INA226_CONFIG_SLOW;
    err = ina226_write(INA226_REG_CONFIG, config & ~7U);
    if (err != ESP_OK) return err;
    return ina226_write(INA226_REG_MASK_ENABLE, INA226_MASK_CNVR);
}

static esp_err_t ina226_trigger(void)
{
    // Vaciar eventos anteriores antes de disparar una conversion nueva.
    ulTaskNotifyTake(pdTRUE, 0);
    const uint64_t start = (uint64_t)esp_timer_get_time();
    const esp_err_t err = ina226_write(INA226_REG_CONFIG,
                                      g_fast_input ? INA226_CONFIG_FAST : INA226_CONFIG_SLOW);
    if (g_timing) g_timing->trigger_us = (uint32_t)((uint64_t)esp_timer_get_time() - start);
    return err;
}

static esp_err_t ina226_collect(input_measurement_t *measurement, uint32_t wait_ms)
{
    const uint64_t wait_start = (uint64_t)esp_timer_get_time();
    const uint32_t ready = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms));
    if (g_timing) g_timing->wait_us = (uint32_t)((uint64_t)esp_timer_get_time() - wait_start);
    if (ready == 0) {
        return ESP_ERR_TIMEOUT;
    }

    const uint64_t read_group_start = (uint64_t)esp_timer_get_time();
    uint16_t status = 0, shunt_raw = 0, bus_raw = 0;
    esp_err_t err;
    if (g_fast_input) {
        err = ina226_bus_read_frame(&status, &shunt_raw, &bus_raw);
        if (g_timing) g_timing->batch_us = (uint32_t)((uint64_t)esp_timer_get_time() - read_group_start);
        if (err != ESP_OK) return err;
        if (!(status & INA226_FLAG_CVRF)) return ESP_ERR_INVALID_STATE;
    } else {
        // Leer Mask/Enable limpia CVRF y libera ALERT.
        uint64_t read_start = (uint64_t)esp_timer_get_time();
        err = ina226_read(INA226_REG_MASK_ENABLE, &status);
        if (g_timing) g_timing->status_us = (uint32_t)((uint64_t)esp_timer_get_time() - read_start);
        if (err != ESP_OK) return err;
        if (!(status & INA226_FLAG_CVRF)) return ESP_ERR_INVALID_STATE;
        // En modo disparado los registros no cambian durante estas lecturas.
        read_start = (uint64_t)esp_timer_get_time();
        err = ina226_read(INA226_REG_SHUNT, &shunt_raw);
        if (g_timing) g_timing->shunt_us = (uint32_t)((uint64_t)esp_timer_get_time() - read_start);
        if (err != ESP_OK) return err;
        read_start = (uint64_t)esp_timer_get_time();
        err = ina226_read(INA226_REG_BUS, &bus_raw);
        if (g_timing) g_timing->bus_us = (uint32_t)((uint64_t)esp_timer_get_time() - read_start);
        if (err != ESP_OK) return err;
    }
    const int16_t shunt_signed = (int16_t)shunt_raw;
    if (shunt_signed == INT16_MAX || shunt_signed == INT16_MIN || bus_raw > 28800U) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    // Escalas del datasheet: 2.5 uV/LSB de shunt y 1.25 mV/LSB de bus.
    // I y P se calculan aqui: no requieren registros Current/Power ni CAL.
    measurement->voltage_v = bus_raw * 0.00125;
    measurement->current_a = shunt_signed * 0.0000025 / INA226_SHUNT_OHMS;
    measurement->power_w = measurement->voltage_v * measurement->current_a;
    if (g_timing) g_timing->read_us = (uint32_t)((uint64_t)esp_timer_get_time() - read_group_start);
    return ESP_OK;
}

static esp_err_t ina226_measure(input_measurement_t *measurement)
{
    const esp_err_t err = ina226_trigger();
    return err == ESP_OK ? ina226_collect(measurement, INA226_READY_TIMEOUT_MS) : err;
}


static QueueHandle_t g_jobs, g_results;

static void acquisition_task(void *arg)
{
    (void)arg;
    input_hardware_init(); // I2C y ALERT instalan sus interrupciones en CPU0.
    acquisition_job_t job;
    while (true) {
        if (xQueueReceive(g_jobs, &job, portMAX_DELAY) != pdTRUE) continue;
        acquisition_result_t result = {.generation = job.generation, .id = job.id,
                                        .core = xPortGetCoreID()};
        const uint64_t start = (uint64_t)esp_timer_get_time();
        result.queue_us = (uint32_t)(start - job.sent_us);
        g_timing = &result;
        if (job.configure) {
            g_fast_input = job.fast;
            result.error = ina226_configure();
        }
        if (result.error == ESP_OK && job.read) {
            acquisition_pipeline_t pipeline;
            input_measurement_t input;
            acquisition_reset(&pipeline);
            // Capturar las condiciones reales al disparar; no se comparten memorias.
            const uint64_t acquisition_start = (uint64_t)esp_timer_get_time();
            acquisition_begin(&pipeline, acquisition_start, job.duty, job.mode);
            result.error = ina226_measure(&input);
            const uint64_t end = (uint64_t)esp_timer_get_time();
            if (result.error == ESP_OK) {
                if (!acquisition_complete(&pipeline, &input, end) ||
                    !acquisition_take(&pipeline, &result.sample)) result.error = ESP_ERR_INVALID_STATE;
            }
            result.total_us = (uint32_t)(end - acquisition_start);
        }
        g_timing = NULL;
        // Solo un trabajo normal en vuelo. Un resultado antiguo se identifica
        // por generacion/id; no bloquea el rearmado despues de un fallo.
        result.published_us = (uint64_t)esp_timer_get_time();
        xQueueOverwrite(g_results, &result);
    }
}

void acquisition_worker_init(void)
{
    g_jobs = xQueueCreate(1, sizeof(acquisition_job_t));
    g_results = xQueueCreate(1, sizeof(acquisition_result_t));
    ESP_ERROR_CHECK(g_jobs && g_results ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(acquisition_task, "ina226_cpu0", 6144, NULL, 12, NULL, 0)
                    == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

bool acquisition_worker_submit(const acquisition_job_t *job, bool discard_queued)
{
    if (discard_queued) xQueueReset(g_jobs);
    return xQueueSend(g_jobs, job, 0) == pdTRUE;
}

bool acquisition_worker_receive(acquisition_result_t *result, uint32_t wait_ms)
{
    return xQueueReceive(g_results, result, pdMS_TO_TICKS(wait_ms)) == pdTRUE;
}

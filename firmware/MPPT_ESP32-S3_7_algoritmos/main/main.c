#include <inttypes.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "mosfet_pwm.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "sdkconfig.h"
#include "mppt_config.h"
#include "mppt_control.h"
#include "serial_link.h"
#include "acquisition_worker.h"
#include "acquisition_gate.h"
#include "environment_sensors.h"
#include "mppt_loads.h"
#include "load_trial_worker.h"

#if CONFIG_FREERTOS_UNICORE
#error "This firmware requires both ESP32-S3 cores"
#endif

_Static_assert(pdMS_TO_TICKS(STOP_ACQUISITION_PERIOD_MS) > 0 &&
               (STOP_ACQUISITION_PERIOD_MS * configTICK_RATE_HZ) % 1000 == 0,
               "La consulta con PWM detenido debe ser representable en ticks");

static const char *TAG = "MPPT_S3";

static void pwm_set_count(uint32_t count)
{
    ESP_ERROR_CHECK(mosfet_pwm_set_count(count));
}

typedef struct {
    uint64_t sequence, deadline_us;
    uint32_t generation;
} release_t;

static QueueHandle_t g_releases;
static portMUX_TYPE g_schedule_mux = portMUX_INITIALIZER_UNLOCKED;
static uint64_t g_schedule_origin, g_schedule_sequence;
static uint32_t g_schedule_period, g_schedule_generation;

static void release_callback(void *arg)
{
    (void)arg;
    release_t release;
    portENTER_CRITICAL(&g_schedule_mux);
    release.sequence = ++g_schedule_sequence;
    release.deadline_us = g_schedule_origin + release.sequence * g_schedule_period;
    release.generation = g_schedule_generation;
    portEXIT_CRITICAL(&g_schedule_mux);
    // Una cola llena deja un salto de secuencia; nunca bloquea al temporizador.
    xQueueSend(g_releases, &release, 0);
}

static void schedule_start(esp_timer_handle_t timer, uint32_t period_us)
{
    xQueueReset(g_releases);
    portENTER_CRITICAL(&g_schedule_mux);
    g_schedule_origin = (uint64_t)esp_timer_get_time();
    g_schedule_sequence = 0;
    g_schedule_period = period_us;
    ++g_schedule_generation;
    portEXIT_CRITICAL(&g_schedule_mux);
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, period_us));
}

// Codigos del adaptador: se mantienen separados de los errores I2C.
#define MPPT_ERR_DEADLINE 0x7001
#define MPPT_ERR_ACQUISITION_DEADLINE 0x7002

static void record_acquisition(serial_sample_t *sample, const acquisition_result_t *result,
                               uint32_t *read_max, uint32_t *acquisition_max, uint64_t deadline)
{
    if (result->read_us > *read_max) *read_max = result->read_us;
    if (result->total_us > *acquisition_max) *acquisition_max = result->total_us;
    sample->read_us = result->read_us; sample->read_max_us = *read_max;
    sample->acquisition_us = result->total_us; sample->acquisition_max_us = *acquisition_max;
    sample->trigger_us = result->trigger_us; sample->wait_us = result->wait_us;
    sample->status_us = result->status_us; sample->shunt_us = result->shunt_us;
    sample->bus_us = result->bus_us; sample->queue_us = result->queue_us;
    sample->batch_us = result->batch_us;
    sample->acquisition_margin_valid = deadline && result->sample.end_us && result->published_us;
    if (sample->acquisition_margin_valid) {
        sample->ready_margin_us = (int32_t)((int64_t)deadline - (int64_t)result->sample.end_us);
        sample->delivery_margin_us = (int32_t)((int64_t)deadline - (int64_t)result->published_us);
    }
    sample->acquisition_core = result->core;
}

static bool receive_matching(acquisition_result_t *result, uint32_t generation,
                              uint64_t id, uint32_t wait_ms)
{
    const uint64_t until = (uint64_t)esp_timer_get_time() + (uint64_t)wait_ms * 1000U;
    do {
        if (!acquisition_worker_receive(result, wait_ms ? 1 : 0)) {
            if (!wait_ms) return false;
        } else if (acquisition_gate(result, generation, id, 0) != ACQUISITION_STALE) return true;
    } while ((uint64_t)esp_timer_get_time() < until);
    return false;
}

static void controller_task(void *arg)
{
    (void)arg;
    g_releases = xQueueCreate(4, sizeof(release_t));
    ESP_ERROR_CHECK(g_releases ? ESP_OK : ESP_ERR_NO_MEM);
    esp_timer_handle_t timer;
    const esp_timer_create_args_t timer_args = {.callback = release_callback, .name = "mppt_release"};
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_LOGI(TAG, "Control CPU%d; adquisicion CPU0; CPU configurada %d MHz; control rapido 2ms",
             xPortGetCoreID(), CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);

    static mppt_control_t control;
    int requested_mode = MODE_PO, applied_mode = MODE_STOP;
    bool restart = true, timer_running = false, fault_latched = false, waiting_prime = false;
    bool fast = false, have_prime = false, acquisition_pending = false;
    uint32_t generation = 0, compute_max = 0, deadline_misses = 0, read_max = 0, acquisition_max = 0;
    uint64_t expected_job = 0, next_report = 0, expected_sequence = 1, previous_start = 0, control_id = 0;
    uint64_t pending_deadline = 0;
    uint32_t pending_budget = 0, pending_compute = 0;
    double applied_duty = 0;
    acquisition_result_t prime = {0};
    serial_sample_t latest = {.control_core = 1, .acquisition_core = -1};

    while (true) {
        serial_command_t command;
        while (serial_link_next_command(&command)) {
            if (timer_running) { ESP_ERROR_CHECK(esp_timer_stop(timer)); timer_running = false; }
            pwm_set_count(0);
            requested_mode = command.mode;
            applied_mode = MODE_STOP; applied_duty = 0;
            fault_latched = false; waiting_prime = false; restart = true; next_report = 0;
            have_prime = false;
            serial_link_confirm(&command);
        }

        if (restart) {
            mppt_control_reset(&control, requested_mode);
            fast = mppt_mode_fast(requested_mode);
            ++generation; expected_job = 0;
            pending_deadline = 0; pending_budget = pending_compute = 0;
            compute_max = read_max = acquisition_max = 0;
            latest = (serial_sample_t){.period_us = mppt_mode_period_us(requested_mode),
                .phase_budget_us = mppt_mode_period_us(requested_mode), .control_core = xPortGetCoreID(),
                .acquisition_core = -1, .deadline_misses = deadline_misses};
            const acquisition_job_t job = {.generation = generation, .id = 0,
                .sent_us = (uint64_t)esp_timer_get_time(), .mode = MODE_STOP, .duty = 0,
                .configure = true, .read = fast, .fast = fast};
            if (!acquisition_worker_submit(&job, true)) {
                latest.error = ESP_ERR_NO_MEM; fault_latched = true;
            } else { waiting_prime = true; acquisition_pending = true; }
            restart = false;
        }

        if (fault_latched) {
            acquisition_result_t result;
            // Si la adquisicion termino tarde, conservar el desglose que explica
            // el fallo. No se presenta ese resultado como una nueva medicion.
            if (acquisition_pending && receive_matching(&result, generation, expected_job, 0)) {
                record_acquisition(&latest, &result, &read_max, &acquisition_max, pending_deadline);
                acquisition_pending = false;
            }
            const uint64_t now = (uint64_t)esp_timer_get_time();
            if (now >= next_report) {
                latest.start_us = latest.end_us = now;
                latest.duty = latest.duty_next = 0;
                latest.mode = latest.mode_next = MODE_STOP; latest.valid = false;
                serial_link_publish(&latest);
                next_report = now + TELEMETRY_PERIOD_US;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (waiting_prime) {
            if (!receive_matching(&prime, generation, 0, 10)) continue;
            acquisition_pending = false;
            record_acquisition(&latest, &prime, &read_max, &acquisition_max, 0);
            if (acquisition_gate(&prime, generation, 0, 0) != ACQUISITION_READY) {
                latest.error = prime.error ? prime.error : ESP_ERR_INVALID_RESPONSE;
                fault_latched = true; waiting_prime = false; continue;
            }
            have_prime = fast;
            expected_sequence = 1; previous_start = 0;
            schedule_start(timer, latest.period_us);
            timer_running = true; waiting_prime = false;
        }

        release_t release;
        if (xQueueReceive(g_releases, &release, pdMS_TO_TICKS(50)) != pdTRUE) continue;
        if (release.generation != g_schedule_generation) continue;
        const uint64_t start = (uint64_t)esp_timer_get_time();
        const uint32_t late = start > release.deadline_us ? (uint32_t)(start - release.deadline_us) : 0;
        const bool missed = release.sequence != expected_sequence || (fast && late > FAST_MAX_LATENESS_US);
        expected_sequence = release.sequence + 1;
        const serial_sample_t previous = latest;
        latest = (serial_sample_t){
            .start_us = start, .end_us = start, .cycle_us = previous_start ? start - previous_start : 0,
            .duty = applied_duty, .mode = applied_mode, .period_us = mppt_mode_period_us(requested_mode),
            .late_us = late, .control_id = ++control_id, .compute_max_us = compute_max,
            .deadline_misses = deadline_misses, .phase_budget_us = mppt_mode_period_us(requested_mode),
            .control_core = xPortGetCoreID(), .acquisition_core = previous.acquisition_core,
            .read_max_us = read_max, .acquisition_max_us = acquisition_max, .timing_phase = fast ? 1 : 0,
            .acquisition_budget_us = pending_budget, .previous_compute_us = pending_compute,
        };
        previous_start = start;
        esp_err_t err = missed && fast ? MPPT_ERR_DEADLINE : ESP_OK;
        acquisition_result_t result = {0};
        if (err == ESP_OK && fast) {
            if (have_prime) { result = prime; have_prime = false; }
            else if (!receive_matching(&result, generation, expected_job, 0)) {
                err = MPPT_ERR_ACQUISITION_DEADLINE;
            }
            if (err == ESP_OK) {
                acquisition_pending = false;
                record_acquisition(&latest, &result, &read_max, &acquisition_max, pending_deadline);
                const acquisition_gate_t gate = acquisition_gate(&result, generation, expected_job, release.deadline_us);
                if (gate == ACQUISITION_ERROR) err = result.error ? result.error : ESP_ERR_INVALID_RESPONSE;
                else if (gate != ACQUISITION_READY) err = MPPT_ERR_ACQUISITION_DEADLINE;
            }
            if (err == MPPT_ERR_ACQUISITION_DEADLINE) latest.timing_phase = 2;
        } else if (err == ESP_OK) {
            // Solo STOP: consulta sincrona del sensor con PWM en cero.
            // El transporte I2C se realiza en la tarea propietaria CPU0.
            const acquisition_job_t job = {.generation = generation, .id = ++expected_job,
                .sent_us = (uint64_t)esp_timer_get_time(), .mode = applied_mode, .duty = applied_duty, .read = true};
            if (!acquisition_worker_submit(&job, false)) err = ESP_ERR_INVALID_STATE;
            else if (!receive_matching(&result, generation, expected_job, 160)) err = ESP_ERR_TIMEOUT;
            else {
                record_acquisition(&latest, &result, &read_max, &acquisition_max, 0);
                if (acquisition_gate(&result, generation, expected_job, 0) != ACQUISITION_READY)
                    err = result.error ? result.error : ESP_ERR_INVALID_RESPONSE;
            }
        }

        latest.valid = err == ESP_OK;
        if (latest.valid) {
            const acquired_sample_t *acquired = &result.sample;
            const input_measurement_t *input = &acquired->input;
            latest.start_us = acquired->start_us; latest.end_us = acquired->end_us;
            latest.duty = acquired->duty; latest.mode = acquired->mode;
            latest.sample_age_us = start > acquired->end_us ? (uint32_t)(start - acquired->end_us) : 0;
            latest.vin = input->voltage_v; latest.iin = input->current_a; latest.pin = input->power_w;
            const uint64_t compute_start = (uint64_t)esp_timer_get_time();
            if (input->voltage_v > 0 && input->current_a >= 0) {
                if (!mppt_control_step(&control, input->voltage_v, input->current_a)) err = ESP_ERR_INVALID_STATE;
            } else mppt_control_reset(&control, requested_mode);
            latest.compute_us = (uint32_t)((uint64_t)esp_timer_get_time() - compute_start);
            if (latest.compute_us > compute_max) compute_max = latest.compute_us;
            latest.compute_max_us = compute_max;
            latest.duty_base = control.base_duty; latest.duty_stage = control.stage_duty;
            latest.phase = fast ? control.imported.output.internal_phase : 0;
            latest.work_us = (uint32_t)((uint64_t)esp_timer_get_time() - start);
            if (fast && late + latest.work_us >= FAST_PERIOD_US) err = MPPT_ERR_DEADLINE;
            if (err == ESP_OK) {
                pwm_set_count(control.pwm_count);
                applied_duty = (double)control.pwm_count / PWM_PERIOD_TICKS;
                applied_mode = control.primed ? requested_mode : MODE_STOP;
                if (fast) {
                    // Lectura inmediata al ALERT en CPU0; no hay espera fija ni
                    // plazo artificial de 1ms. Debe estar lista en el proximo tick de 2ms.
                    const acquisition_job_t job = {.generation = generation, .id = ++expected_job,
                        .sent_us = (uint64_t)esp_timer_get_time(), .mode = applied_mode,
                        .duty = applied_duty, .read = true, .fast = true};
                    pending_deadline = release.deadline_us + FAST_PERIOD_US;
                    pending_budget = pending_deadline > job.sent_us ? (uint32_t)(pending_deadline - job.sent_us) : 0;
                    pending_compute = latest.compute_us;
                    if (!acquisition_worker_submit(&job, false)) err = ESP_ERR_INVALID_STATE;
                    else acquisition_pending = true;
                }
            }
        }
        latest.work_us = (uint32_t)((uint64_t)esp_timer_get_time() - start);
        if (fast && late + latest.work_us >= FAST_PERIOD_US) err = MPPT_ERR_DEADLINE;
        if (err != ESP_OK) {
            pwm_set_count(0); applied_mode = MODE_STOP; applied_duty = 0;
            ESP_ERROR_CHECK(esp_timer_stop(timer)); timer_running = false;
            fault_latched = true; latest.error = err;
            latest.timing_fault = err == MPPT_ERR_DEADLINE || err == MPPT_ERR_ACQUISITION_DEADLINE;
            if (latest.timing_fault) ++deadline_misses;
            latest.deadline_misses = deadline_misses;
        }
        latest.duty_next = applied_duty; latest.mode_next = applied_mode;
        const uint64_t now = (uint64_t)esp_timer_get_time();
        if (now >= next_report || err != ESP_OK) {
            serial_link_publish(&latest); next_report = now + TELEMETRY_PERIOD_US;
        }
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(mosfet_pwm_init());
    ESP_ERROR_CHECK(mppt_loads_init());
    environment_sensors_init();
    serial_link_init();
    load_trial_worker_init();
    acquisition_worker_init();
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(controller_task, "mppt_cpu1", 12288, NULL, 12, NULL, 1)
                    == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

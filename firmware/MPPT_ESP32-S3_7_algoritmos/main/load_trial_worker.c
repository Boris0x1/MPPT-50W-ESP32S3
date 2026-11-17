#include "load_trial_worker.h"
#include "mppt_loads.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

typedef struct { uint64_t observed_us; int mode; bool healthy; } control_status_t;
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static load_trial_t g_snapshot;
static control_status_t g_control;
static unsigned g_pending_starts;
static bool g_operator_stop;
static QueueHandle_t g_requests;

load_trial_t load_trial_snapshot(void)
{
    portENTER_CRITICAL(&g_mux);
    const load_trial_t out = g_snapshot;
    portEXIT_CRITICAL(&g_mux);
    return out;
}

bool load_trial_busy(void)
{
    portENTER_CRITICAL(&g_mux);
    const bool busy = load_trial_active(&g_snapshot) || g_pending_starts;
    portEXIT_CRITICAL(&g_mux);
    return busy;
}

void load_trial_observe(const serial_sample_t *sample, uint64_t now)
{
    const control_status_t status = {.observed_us = now, .mode = sample->mode_next,
        .healthy = sample->valid && !sample->error && !sample->timing_fault};
    portENTER_CRITICAL(&g_mux); g_control = status; portEXIT_CRITICAL(&g_mux);
}

static control_status_t control_snapshot(void)
{
    portENTER_CRITICAL(&g_mux); const control_status_t out = g_control; portEXIT_CRITICAL(&g_mux);
    return out;
}

static void publish(const load_trial_t *trial)
{
    portENTER_CRITICAL(&g_mux); g_snapshot = *trial; portEXIT_CRITICAL(&g_mux);
}

bool load_trial_submit(const serial_command_t *command)
{
    if (!g_requests) return false;
    if (command->kind == SERIAL_TRIAL_START) {
        portENTER_CRITICAL(&g_mux); ++g_pending_starts; portEXIT_CRITICAL(&g_mux);
    }
    if (xQueueSend(g_requests, command, 0) == pdTRUE) return true;
    if (command->kind == SERIAL_TRIAL_START) {
        portENTER_CRITICAL(&g_mux); --g_pending_starts; portEXIT_CRITICAL(&g_mux);
    }
    return false;
}

void load_trial_operator_stop(void)
{
    portENTER_CRITICAL(&g_mux); g_operator_stop = true; portEXIT_CRITICAL(&g_mux);
}

static void trial_task(void *arg)
{
    (void)arg;
    load_trial_t trial = {0};
    uint32_t next_id = 0;
    char last_request[33] = "";
    uint64_t next_stop_request = 0;
    while (true) {
        portENTER_CRITICAL(&g_mux);
        const bool operator_stop = g_operator_stop; g_operator_stop = false;
        portEXIT_CRITICAL(&g_mux);
        if (operator_stop) { load_trial_cancel(&trial); next_stop_request = 0; }
        serial_command_t request;
        if (xQueueReceive(g_requests, &request, pdMS_TO_TICKS(10)) == pdTRUE) {
            bool accepted = false;
            if (request.kind == SERIAL_LOAD) {
                accepted = !load_trial_active(&trial) && mppt_loads_select(request.ohms, NULL) == ESP_OK;
            } else if (request.kind == SERIAL_TRIAL_START) {
                const control_status_t control = control_snapshot();
                const uint64_t now = (uint64_t)esp_timer_get_time();
                // Una retransmision con el mismo ID no reinicia el ensayo ni el reloj.
                if (!strcmp(last_request, request.id)) {
                    accepted = trial.mode == request.mode && trial.interval_ms == request.interval_ms;
                } else if (request.mode >= 2 && request.mode <= 8 && request.interval_ms >= 1000 &&
                           request.interval_ms <= 3600000 && request.interval_ms % 1000 == 0 &&
                           !load_trial_active(&trial) && control.healthy && control.mode == request.mode &&
                           control.observed_us && now >= control.observed_us && now - control.observed_us <= 500000U &&
                           mppt_loads_select(10, NULL) == ESP_OK) {
                    if (++next_id == 0) ++next_id;
                    accepted = load_trial_begin(&trial, next_id, request.mode, request.interval_ms,
                                                 (uint64_t)esp_timer_get_time());
                    if (accepted) { strcpy(last_request, request.id); next_stop_request = 0; }
                }
                request.trial_id = trial.id;
                publish(&trial);
                portENTER_CRITICAL(&g_mux); --g_pending_starts; portEXIT_CRITICAL(&g_mux);
            } else if (request.kind == SERIAL_TRIAL_CANCEL) {
                accepted = trial.id && (request.internal || request.trial_id == trial.id);
                if (accepted) { load_trial_cancel(&trial); next_stop_request = 0; }
                request.trial_id = trial.id;
            }
            publish(&trial);
            if (!request.internal) serial_link_reply(&request, accepted);
        }
        const uint64_t now = (uint64_t)esp_timer_get_time();
        const control_status_t control = control_snapshot();
        mppt_loads_state_t load; mppt_loads_get_state(&load);
        const bool recent = control.observed_us && now >= control.observed_us && now - control.observed_us <= 500000U;
        const int action = load_trial_poll(&trial, now, control.mode, recent && control.healthy,
            load.active && load.ohms == load_trial_sequence[trial.stage]);
        if (action == TRIAL_ACTION_LOAD) {
            if (mppt_loads_select(load_trial_sequence[trial.stage + 1U], NULL) == ESP_OK)
                load_trial_commit_load(&trial);
            else load_trial_fail(&trial, TRIAL_ERROR_LOAD);
        }
        if (trial.status == TRIAL_STOPPING) {
            if (recent && control.mode == 0) load_trial_confirm_stop(&trial, now);
            else if (now >= next_stop_request) {
                serial_link_request_stop(); next_stop_request = now + 100000U;
            }
        }
        publish(&trial);
    }
}

void load_trial_worker_init(void)
{
    g_requests = xQueueCreate(8, sizeof(serial_command_t));
    ESP_ERROR_CHECK(g_requests ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(trial_task, "loads_cpu0", 4096, NULL, 2, NULL, 0)
        == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

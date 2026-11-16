#include "serial_link.h"
#include "mppt_control.h"
#include "environment_sensors.h"
#include "mppt_loads.h"
#include "load_trial_worker.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "driver/usb_serial_jtag.h"
#include "esp_err.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

typedef struct {
    bool acknowledgement;
    bool accepted;
    uint64_t sample_id;
    serial_command_t command;
    serial_sample_t sample;
    environment_snapshot_t environment;
    mppt_loads_state_t load;
    load_trial_t trial;
} tx_message_t;

typedef struct {
    char data[160];
    size_t used;
    bool overflow;
} rx_line_t;

static QueueHandle_t commands, outbound;
static uint64_t boot_id, sample_id;

static void queue_ack(const serial_command_t *command, bool accepted)
{
    const tx_message_t message = {
        .acknowledgement = true, .accepted = accepted, .command = *command,
    };
    // Si se pierde un ACK, la web informa timeout; no inventa confirmaciones.
    xQueueSend(outbound, &message, 0);
}

static void parse_command(const char *line, bool usb)
{
    cJSON *json = cJSON_Parse(line);
    if (!json) return;
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "type");
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id");
    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(json, "mode");
    const cJSON *ohms = cJSON_GetObjectItemCaseSensitive(json, "ohms");
    const bool valid_id = cJSON_IsString(id) && strlen(id->valuestring) > 0 &&
        strlen(id->valuestring) <= 32 && strspn(id->valuestring, "0123456789") == strlen(id->valuestring);
    if (cJSON_IsString(type) && valid_id &&
        (!strcmp(type->valuestring, "start_trial") || !strcmp(type->valuestring, "cancel_trial"))) {
        serial_command_t command = {.usb = usb, .kind = !strcmp(type->valuestring, "start_trial") ?
            SERIAL_TRIAL_START : SERIAL_TRIAL_CANCEL};
        snprintf(command.id, sizeof(command.id), "%s", id->valuestring);
        const cJSON *interval = cJSON_GetObjectItemCaseSensitive(json, "interval_ms");
        const cJSON *trial_id = cJSON_GetObjectItemCaseSensitive(json, "trial_id");
        bool accepted;
        if (command.kind == SERIAL_TRIAL_START) {
            accepted = cJSON_IsNumber(mode) && mode->valuedouble == mode->valueint &&
                mode->valueint >= MODE_PO_ADJUSTED && mode->valueint <= MODE_COMBINATION && cJSON_IsNumber(interval) &&
                interval->valuedouble == interval->valueint && interval->valueint >= 1000 &&
                interval->valueint <= 3600000 && interval->valueint % 1000 == 0;
            if (accepted) { command.mode = mode->valueint; command.interval_ms = interval->valueint; }
        } else {
            accepted = cJSON_IsNumber(trial_id) && trial_id->valuedouble >= 1 &&
                trial_id->valuedouble <= UINT32_MAX && trial_id->valuedouble == (uint32_t)trial_id->valuedouble;
            if (accepted) command.trial_id = (uint32_t)trial_id->valuedouble;
        }
        if (!accepted || !load_trial_submit(&command)) queue_ack(&command, false);
        cJSON_Delete(json); return;
    }
    if (cJSON_IsString(type) && strcmp(type->valuestring, "set_load") == 0 &&
        cJSON_IsString(id) && strlen(id->valuestring) > 0 && strlen(id->valuestring) <= 32 &&
        strspn(id->valuestring, "0123456789") == strlen(id->valuestring)) {
        serial_command_t command = {.kind = SERIAL_LOAD, .usb = usb, .ohms = -1};
        snprintf(command.id, sizeof(command.id), "%s", id->valuestring);
        bool accepted = cJSON_IsNumber(ohms) && ohms->valuedouble == ohms->valueint &&
                        (ohms->valueint == 10 || ohms->valueint == 20 || ohms->valueint == 30);
        if (accepted) {
            command.ohms = ohms->valueint;
        }
        if (!accepted || !load_trial_submit(&command)) queue_ack(&command, false);
        cJSON_Delete(json);
        return;
    }
    if (cJSON_IsString(type) && strcmp(type->valuestring, "set_mode") == 0 &&
        cJSON_IsString(id) && strlen(id->valuestring) > 0 && strlen(id->valuestring) <= 32) {
        serial_command_t command = {.mode = -1, .usb = usb};
        // IDs sencillos permiten serializarlos sin interpolar texto arbitrario.
        if (strspn(id->valuestring, "0123456789") != strlen(id->valuestring)) {
            cJSON_Delete(json);
            return;
        }
        snprintf(command.id, sizeof(command.id), "%s", id->valuestring);
        if (cJSON_IsNumber(mode) && mode->valuedouble >= MODE_STOP &&
            mode->valuedouble <= MODE_COMBINATION && mode->valuedouble == mode->valueint && mppt_mode_valid(mode->valueint)) {
            command.mode = mode->valueint;
            if (load_trial_busy() && command.mode != MODE_STOP) queue_ack(&command, false);
            else {
                if (command.mode == MODE_STOP) load_trial_operator_stop();
                if (xQueueSend(commands, &command, 0) != pdTRUE) queue_ack(&command, false);
            }
        } else queue_ack(&command, false);
    }
    cJSON_Delete(json);
}

static void receive_bytes(rx_line_t *line, const uint8_t *data, int size, bool usb)
{
    for (int i = 0; i < size; ++i) {
        const char value = (char)data[i];
        if (value == '\n') {
            if (!line->overflow && line->used) {
                line->data[line->used] = '\0';
                parse_command(line->data, usb);
            }
            line->used = 0;
            line->overflow = false;
        } else if (value != '\r' && !line->overflow) {
            if (line->used < sizeof(line->data) - 1) line->data[line->used++] = value;
            else line->overflow = true;
        }
    }
}

static void receive_task(void *arg)
{
    (void)arg;
    rx_line_t uart_line = {0}, usb_line = {0};
    uint8_t bytes[128];
    while (true) {
        int size = uart_read_bytes(UART_NUM_0, bytes, sizeof(bytes), 0);
        receive_bytes(&uart_line, bytes, size, false);
        size = usb_serial_jtag_read_bytes(bytes, sizeof(bytes), 0);
        receive_bytes(&usb_line, bytes, size, true);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void send_line(const char *line, bool uart, bool usb)
{
    const size_t length = strlen(line);
    if (uart) uart_write_bytes(UART_NUM_0, line, length);
    if (usb && usb_serial_jtag_is_connected()) {
        // La espera pertenece al transporte, nunca a la tarea del MPPT.
        usb_serial_jtag_write_bytes(line, length, pdMS_TO_TICKS(20));
    }
}

static void transmit_task(void *arg)
{
    (void)arg;
    tx_message_t message;
    char line[2048];
    while (true) {
        if (xQueueReceive(outbound, &message, portMAX_DELAY) != pdTRUE) continue;
        if (message.acknowledgement) {
            if (message.command.kind == SERIAL_TRIAL_START || message.command.kind == SERIAL_TRIAL_CANCEL) {
                snprintf(line, sizeof(line),
                    "{\"type\":\"ack\",\"id\":\"%s\",\"ok\":%s,\"trial_id\":%" PRIu32 ",\"op\":\"%s\"}\n",
                    message.command.id, message.accepted ? "true" : "false", message.command.trial_id,
                    message.command.kind == SERIAL_TRIAL_START ? "start_trial" : "cancel_trial");
                send_line(line, !message.command.usb, message.command.usb); continue;
            }
            if (message.command.kind == SERIAL_LOAD) {
                snprintf(line, sizeof(line),
                         "{\"type\":\"ack\",\"id\":\"%s\",\"ok\":%s,\"load_ohms\":%d}\n",
                         message.command.id, message.accepted ? "true" : "false", message.command.ohms);
                send_line(line, !message.command.usb, message.command.usb);
                continue;
            }
            snprintf(line, sizeof(line),
                     "{\"type\":\"ack\",\"id\":\"%s\",\"ok\":%s,\"mode\":%d}\n",
                     message.command.id, message.accepted ? "true" : "false", message.command.mode);
            send_line(line, !message.command.usb, message.command.usb);
        } else {
            const serial_sample_t *s = &message.sample;
            char environment[320];
            if (!environment_format_json(environment, sizeof(environment), &message.environment)) continue;
            char load[120];
            if (message.load.ready) snprintf(load, sizeof(load),
                "\"ld\":[%u,%u,%" PRIu32 ",\"%" PRIu64 "\",%s]",
                (unsigned)message.load.ohms, (unsigned)message.load.gpio, message.load.revision,
                message.load.switched_after_us, message.load.active ? "true" : "false");
            else snprintf(load, sizeof(load), "\"ld\":null");
            char trial[180];
            const load_trial_t *t = &message.trial;
            uint64_t elapsed_end = t->ended_us ? t->ended_us : message.environment.snapshot_us;
            if (t->id) snprintf(trial, sizeof(trial),
                "\"tr\":[%" PRIu32 ",%u,%u,%u,%" PRIu32 ",%" PRIu64 ",\"%" PRIu64 "\",\"%" PRIu64 "\",%u]",
                t->id, t->status, t->stage, t->mode, t->interval_ms,
                elapsed_end >= t->started_us ? (elapsed_end - t->started_us) / 1000U : 0,
                t->started_us, t->ended_us, t->error);
            else snprintf(trial, sizeof(trial), "\"tr\":null");
            char registers[100] = "";
            if (!s->batch_us) snprintf(registers, sizeof(registers),
                "\"status_us\":%" PRIu32 ",\"shunt_us\":%" PRIu32 ",\"bus_us\":%" PRIu32 ",",
                s->status_us, s->shunt_us, s->bus_us);
            char electrical[100];
            if (s->valid) snprintf(electrical, sizeof(electrical),
                                  "\"vin\":%.5f,\"iin\":%.6f,\"pin\":%.5f",
                                  (double)s->vin, (double)s->iin, (double)s->pin);
            else snprintf(electrical, sizeof(electrical), "\"vin\":null,\"iin\":null,\"pin\":null");
            // El ID se asigna al adquirir, no al transmitir: los saltos revelan perdidas.
            snprintf(line, sizeof(line),
                     "{\"type\":\"sample\",\"schema\":1,\"device\":\"esp32s3\","
                     "\"boot\":\"%016" PRIx64 "\",\"id\":\"%" PRIu64 "\","
                     "\"start_us\":\"%" PRIu64 "\",\"end_us\":\"%" PRIu64 "\","
                     "\"cycle_us\":%" PRIu64 ",%s,\"duty\":%.6f,\"duty_next\":%.6f,"
                     "\"mode\":%d,\"mode_next\":%d,\"valid\":%s,\"error\":%d,"
                     "%s,%s,%s,\"control_id\":\"%" PRIu64 "\","
                     "\"period_us\":%" PRIu32 ",\"compute_us\":%" PRIu32 ","
                     "\"compute_max_us\":%" PRIu32 ",\"work_us\":%" PRIu32 ","
                     "\"late_us\":%" PRIu32 ",\"deadline_misses\":%" PRIu32 ","
                     "\"timing_fault\":%s,\"phase\":%d,\"duty_base\":%.8f,\"duty_stage\":%.8f,"
                     "\"read_us\":%" PRIu32 ",\"read_max_us\":%" PRIu32 ","
                     "\"sample_age_us\":%" PRIu32 ",\"timing_phase\":%d,"
                     "\"acquisition_us\":%" PRIu32 ",\"acquisition_max_us\":%" PRIu32 ","
                     "\"trigger_us\":%" PRIu32 ",\"wait_us\":%" PRIu32 ","
                     "%s\"queue_us\":%" PRIu32 ","
                     "\"control_core\":%d,\"acquisition_core\":%d,"
                     "\"batch_us\":%" PRIu32 ",\"acquisition_budget_us\":%" PRIu32 ","
                     "\"previous_compute_us\":%" PRIu32 ",\"ready_margin_us\":%" PRId32 ","
                     "\"delivery_margin_us\":%" PRId32 ",\"acquisition_margin_valid\":%s}\n",
                     boot_id, message.sample_id,
                     s->start_us, s->end_us, s->cycle_us, electrical,
                     (double)s->duty, (double)s->duty_next, s->mode, s->mode_next,
                     s->valid ? "true" : "false", s->error, environment, load, trial, s->control_id,
                     s->period_us, s->compute_us, s->compute_max_us, s->work_us,
                     s->late_us, s->deadline_misses, s->timing_fault ? "true" : "false",
                     s->phase, s->duty_base, s->duty_stage,
                     s->read_us, s->read_max_us, s->sample_age_us, s->timing_phase,
                     s->acquisition_us, s->acquisition_max_us, s->trigger_us, s->wait_us,
                     registers, s->queue_us, s->control_core, s->acquisition_core,
                     s->batch_us, s->acquisition_budget_us, s->previous_compute_us,
                     s->ready_margin_us, s->delivery_margin_us, s->acquisition_margin_valid ? "true" : "false");
            send_line(line, true, true);
        }
    }
}

void serial_link_init(void)
{
    boot_id = ((uint64_t)esp_random() << 32) | esp_random();
    commands = xQueueCreate(8, sizeof(serial_command_t));
    outbound = xQueueCreate(32, sizeof(tx_message_t));
    ESP_ERROR_CHECK(commands && outbound ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 2048, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_set_baudrate(UART_NUM_0, 115200));
    uart_vfs_dev_use_driver(UART_NUM_0);
    usb_serial_jtag_driver_config_t usb_config = {.tx_buffer_size = 2048, .rx_buffer_size = 1024};
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_config));
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(receive_task, "serial_rx", 4096, NULL, 2, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(transmit_task, "serial_tx", 8192, NULL, 1, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

bool serial_link_next_command(serial_command_t *command)
{
    return xQueueReceive(commands, command, 0) == pdTRUE;
}

void serial_link_confirm(const serial_command_t *command)
{
    if (!command->internal) queue_ack(command, true);
}

void serial_link_reply(const serial_command_t *command, bool accepted) { queue_ack(command, accepted); }

bool serial_link_request_stop(void)
{
    const serial_command_t command = {.kind = SERIAL_MODE, .mode = MODE_STOP, .internal = true};
    return xQueueSend(commands, &command, 0) == pdTRUE;
}

void serial_link_publish(const serial_sample_t *sample)
{
    tx_message_t message = {.sample = *sample, .sample_id = ++sample_id};
    message.environment = environment_sensors_snapshot((uint64_t)esp_timer_get_time());
    mppt_loads_get_state(&message.load);
    load_trial_observe(sample, (uint64_t)esp_timer_get_time());
    message.trial = load_trial_snapshot();
    xQueueSend(outbound, &message, 0);
}

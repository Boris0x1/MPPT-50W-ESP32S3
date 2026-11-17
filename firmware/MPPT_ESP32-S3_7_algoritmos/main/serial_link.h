#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum { SERIAL_MODE, SERIAL_LOAD, SERIAL_TRIAL_START, SERIAL_TRIAL_CANCEL } serial_command_kind_t;
typedef struct {
    char id[33];
    int mode;
    bool usb;
    serial_command_kind_t kind;
    bool internal;
    int ohms;
    uint32_t interval_ms, trial_id;
} serial_command_t;

typedef struct {
    uint64_t start_us, end_us, cycle_us;
    double vin, iin, pin, duty, duty_next;
    double duty_base, duty_stage;
    uint64_t control_id;
    uint32_t period_us, compute_us, compute_max_us, work_us, late_us, deadline_misses;
    uint32_t phase_budget_us, read_us, read_max_us, read_late_us, sample_age_us;
    uint32_t acquisition_us, acquisition_max_us, trigger_us, wait_us, status_us, shunt_us, bus_us, queue_us;
    uint32_t batch_us, acquisition_budget_us, previous_compute_us;
    int32_t ready_margin_us, delivery_margin_us;
    bool acquisition_margin_valid;
    int control_core, acquisition_core;
    int phase, timing_phase; // 0: ciclo completo; 1: control/disparo; 2: lectura.
    bool timing_fault;
    int mode, mode_next;
    bool valid;
    int error;
} serial_sample_t;

void serial_link_init(void);
bool serial_link_next_command(serial_command_t *command);
void serial_link_confirm(const serial_command_t *command);
void serial_link_reply(const serial_command_t *command, bool accepted);
bool serial_link_request_stop(void);
void serial_link_publish(const serial_sample_t *sample);

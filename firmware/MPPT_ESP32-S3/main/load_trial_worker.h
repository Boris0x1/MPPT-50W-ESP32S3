#pragma once
#include "load_trial.h"
#include "serial_link.h"

void load_trial_worker_init(void);
bool load_trial_submit(const serial_command_t *command);
bool load_trial_busy(void);
void load_trial_operator_stop(void);
void load_trial_observe(const serial_sample_t *sample, uint64_t now_us);
load_trial_t load_trial_snapshot(void);

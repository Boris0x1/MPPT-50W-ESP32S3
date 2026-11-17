#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { TRIAL_IDLE, TRIAL_RUNNING, TRIAL_STOPPING, TRIAL_COMPLETED, TRIAL_CANCELLED, TRIAL_FAILED };
enum { TRIAL_ERROR_NONE, TRIAL_ERROR_CONTROL, TRIAL_ERROR_LOAD, TRIAL_ERROR_DEADLINE, TRIAL_ERROR_MODE };
enum { TRIAL_ACTION_NONE, TRIAL_ACTION_LOAD, TRIAL_ACTION_STOP };
#define LOAD_TRIAL_STAGES 10
extern const uint8_t load_trial_sequence[LOAD_TRIAL_STAGES];

typedef struct {
    uint32_t id, interval_ms;
    uint64_t started_us, ended_us;
    uint8_t status, stage, mode, error, terminal_status;
} load_trial_t;

bool load_trial_active(const load_trial_t *trial);
bool load_trial_begin(load_trial_t *trial, uint32_t id, int mode, uint32_t interval_ms, uint64_t now_us);
int load_trial_poll(load_trial_t *trial, uint64_t now_us, int mode, bool healthy, bool load_matches);
void load_trial_commit_load(load_trial_t *trial);
void load_trial_cancel(load_trial_t *trial);
void load_trial_fail(load_trial_t *trial, int error);
void load_trial_confirm_stop(load_trial_t *trial, uint64_t now_us);

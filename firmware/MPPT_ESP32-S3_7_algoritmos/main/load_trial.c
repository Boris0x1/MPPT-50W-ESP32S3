#include "load_trial.h"

const uint8_t load_trial_sequence[LOAD_TRIAL_STAGES] = {10,20,30,10,20,30,10,20,30,10};

bool load_trial_active(const load_trial_t *t)
{
    return t->status == TRIAL_RUNNING || t->status == TRIAL_STOPPING;
}

bool load_trial_begin(load_trial_t *t, uint32_t id, int mode, uint32_t interval, uint64_t now)
{
    if (load_trial_active(t) || !id || mode < 2 || mode > 8 || interval < 1000 ||
        interval > 3600000 || interval % 1000) return false;
    *t = (load_trial_t){.id = id, .mode = mode, .interval_ms = interval,
        .started_us = now, .status = TRIAL_RUNNING};
    return true;
}

void load_trial_fail(load_trial_t *t, int error)
{
    if (!load_trial_active(t)) return;
    t->error = error; t->terminal_status = TRIAL_FAILED; t->status = TRIAL_STOPPING;
}

void load_trial_cancel(load_trial_t *t)
{
    if (!load_trial_active(t)) return;
    if (t->terminal_status != TRIAL_FAILED) t->terminal_status = TRIAL_CANCELLED;
    t->status = TRIAL_STOPPING;
}

int load_trial_poll(load_trial_t *t, uint64_t now, int mode, bool healthy, bool load_matches)
{
    if (t->status == TRIAL_STOPPING) return TRIAL_ACTION_STOP;
    if (t->status != TRIAL_RUNNING) return TRIAL_ACTION_NONE;
    if (!healthy) load_trial_fail(t, TRIAL_ERROR_CONTROL);
    else if (mode != t->mode) load_trial_fail(t, TRIAL_ERROR_MODE);
    else if (!load_matches) load_trial_fail(t, TRIAL_ERROR_LOAD);
    else if (now < t->started_us) load_trial_fail(t, TRIAL_ERROR_DEADLINE);
    if (t->status == TRIAL_STOPPING) return TRIAL_ACTION_STOP;
    const uint64_t boundary = t->started_us + (uint64_t)(t->stage + 1U) * t->interval_ms * 1000U;
    if (now < boundary) return TRIAL_ACTION_NONE;
    // No recuperar etapas perdidas con cambios de carga consecutivos.
    if (now - boundary > 100000U) load_trial_fail(t, TRIAL_ERROR_DEADLINE);
    else if (t->stage == LOAD_TRIAL_STAGES - 1) {
        t->terminal_status = TRIAL_COMPLETED; t->status = TRIAL_STOPPING;
    } else return TRIAL_ACTION_LOAD;
    return TRIAL_ACTION_STOP;
}

void load_trial_commit_load(load_trial_t *t)
{
    if (t->status == TRIAL_RUNNING && t->stage + 1 < LOAD_TRIAL_STAGES) ++t->stage;
}

void load_trial_confirm_stop(load_trial_t *t, uint64_t now)
{
    if (t->status != TRIAL_STOPPING || now < t->started_us) return;
    t->ended_us = now; t->status = t->terminal_status;
}

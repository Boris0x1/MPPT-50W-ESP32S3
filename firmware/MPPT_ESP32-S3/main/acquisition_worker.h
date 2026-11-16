#pragma once

#include "acquisition_pipeline.h"

typedef struct {
    uint32_t generation;
    uint64_t id, sent_us;
    int mode;
    double duty;
    bool configure, read, fast;
} acquisition_job_t;

typedef struct {
    uint32_t generation;
    uint64_t id, published_us;
    acquired_sample_t sample;
    uint32_t trigger_us, wait_us, status_us, shunt_us, bus_us, read_us, queue_us, total_us;
    uint32_t batch_us;
    int error, core;
} acquisition_result_t;

void acquisition_worker_init(void);
bool acquisition_worker_submit(const acquisition_job_t *job, bool discard_queued);
bool acquisition_worker_receive(acquisition_result_t *result, uint32_t wait_ms);

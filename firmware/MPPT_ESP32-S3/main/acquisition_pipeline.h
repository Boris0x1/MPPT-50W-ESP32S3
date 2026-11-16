#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    double voltage_v, current_a, power_w;
} input_measurement_t;

typedef struct {
    input_measurement_t input;
    uint64_t start_us, end_us;
    double duty;
    int mode;
} acquired_sample_t;

// Un solo propietario por instancia; el worker INA226 usa una instancia local.
typedef struct {
    acquired_sample_t sample;
    bool pending, ready;
} acquisition_pipeline_t;

void acquisition_reset(acquisition_pipeline_t *pipeline);
bool acquisition_begin(acquisition_pipeline_t *pipeline, uint64_t start_us, double duty, int mode);
bool acquisition_complete(acquisition_pipeline_t *pipeline, const input_measurement_t *input, uint64_t end_us);
bool acquisition_take(acquisition_pipeline_t *pipeline, acquired_sample_t *sample);

#include "acquisition_pipeline.h"

#include <string.h>

void acquisition_reset(acquisition_pipeline_t *pipeline)
{
    memset(pipeline, 0, sizeof(*pipeline));
}

bool acquisition_begin(acquisition_pipeline_t *pipeline, uint64_t start_us, double duty, int mode)
{
    if (pipeline->pending || pipeline->ready) return false;
    pipeline->sample = (acquired_sample_t){.start_us = start_us, .duty = duty, .mode = mode};
    pipeline->pending = true;
    return true;
}

bool acquisition_complete(acquisition_pipeline_t *pipeline, const input_measurement_t *input, uint64_t end_us)
{
    if (!pipeline->pending || pipeline->ready || end_us < pipeline->sample.start_us) return false;
    pipeline->sample.input = *input;
    pipeline->sample.end_us = end_us;
    pipeline->pending = false;
    pipeline->ready = true;
    return true;
}

bool acquisition_take(acquisition_pipeline_t *pipeline, acquired_sample_t *sample)
{
    if (!pipeline->ready || pipeline->pending) return false;
    *sample = pipeline->sample;
    pipeline->ready = false;
    return true;
}

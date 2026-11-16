#include "acquisition_gate.h"

acquisition_gate_t acquisition_gate(const acquisition_result_t *result, uint32_t generation,
                                    uint64_t id, uint64_t deadline_us)
{
    if (result->generation != generation || result->id != id) return ACQUISITION_STALE;
    if (result->error || result->sample.end_us < result->sample.start_us) return ACQUISITION_ERROR;
    if (deadline_us && result->sample.end_us > deadline_us) return ACQUISITION_LATE;
    return ACQUISITION_READY;
}

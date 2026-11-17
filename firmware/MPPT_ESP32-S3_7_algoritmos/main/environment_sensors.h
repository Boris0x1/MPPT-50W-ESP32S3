#pragma once
#include "environment_state.h"

void environment_sensors_init(void);
environment_snapshot_t environment_sensors_snapshot(uint64_t now_us);

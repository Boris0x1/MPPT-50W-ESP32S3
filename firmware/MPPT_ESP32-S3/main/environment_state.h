#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ENV_TEMPERATURE_COUNT 4
#define ENV_NO_AGE UINT32_MAX

typedef struct {
    uint64_t irr_acquired_us, temp_acquired_us;
    uint16_t irradiance;
    float temperatures[ENV_TEMPERATURE_COUNT];
    int irr_error, temp_errors[ENV_TEMPERATURE_COUNT];
    bool irr_valid;
    uint8_t temp_mask;
} environment_state_t;

typedef struct {
    uint64_t snapshot_us;
    float irradiance, temperature, temperatures[ENV_TEMPERATURE_COUNT];
    uint32_t irr_age_ms, temp_age_ms;
    int irr_error, temp_errors[ENV_TEMPERATURE_COUNT];
    uint8_t temp_mask;
} environment_snapshot_t;

void environment_state_init(environment_state_t *state);
void environment_store_irradiance(environment_state_t *state, uint16_t value, int error, uint64_t now_us);
void environment_store_temperatures(environment_state_t *state, const float values[4], const int errors[4], uint64_t now_us);
environment_snapshot_t environment_snapshot(const environment_state_t *state, uint64_t now_us);
uint16_t environment_modbus_crc(const uint8_t *data, size_t size);
bool environment_decode_irradiance(const uint8_t *frame, size_t size, uint16_t *value);
// Fragmento JSON sin llaves exteriores. Devuelve 0 si no cabe completo.
size_t environment_format_json(char *buffer, size_t size, const environment_snapshot_t *snapshot);

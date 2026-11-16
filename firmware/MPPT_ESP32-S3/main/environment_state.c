#include "environment_state.h"
#include "mppt_config.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

void environment_state_init(environment_state_t *s)
{
    memset(s, 0, sizeof(*s));
    s->irr_error = 0x103; // ESP_ERR_INVALID_STATE: todavia sin lectura.
    for (unsigned i = 0; i < 4; ++i) { s->temperatures[i] = NAN; s->temp_errors[i] = 0x103; }
}

void environment_store_irradiance(environment_state_t *s, uint16_t value, int error, uint64_t now)
{
    s->irr_valid = error == 0;
    s->irr_error = error;
    if (s->irr_valid) { s->irradiance = value; s->irr_acquired_us = now; }
}

void environment_store_temperatures(environment_state_t *s, const float values[4], const int errors[4], uint64_t now)
{
    s->temp_mask = 0;
    for (unsigned i = 0; i < 4; ++i) {
        const bool valid = !errors[i] && isfinite(values[i]) && values[i] >= -55 && values[i] <= 125;
        s->temperatures[i] = valid ? values[i] : NAN;
        s->temp_errors[i] = errors[i] ? errors[i] : valid ? 0 : 0x108;
        if (valid) s->temp_mask |= 1U << i;
    }
    if (s->temp_mask) s->temp_acquired_us = now;
}

static uint32_t age_ms(uint64_t now, uint64_t acquired)
{
    if (!acquired || now < acquired) return ENV_NO_AGE;
    const uint64_t age = (now - acquired) / 1000;
    return age >= ENV_NO_AGE ? ENV_NO_AGE : (uint32_t)age;
}

environment_snapshot_t environment_snapshot(const environment_state_t *s, uint64_t now)
{
    environment_snapshot_t out = {.snapshot_us = now, .irradiance = NAN, .temperature = NAN,
        .irr_age_ms = age_ms(now, s->irr_acquired_us), .temp_age_ms = age_ms(now, s->temp_acquired_us),
        .irr_error = s->irr_error};
    if (s->irr_valid && out.irr_age_ms != ENV_NO_AGE && now - s->irr_acquired_us <= ENV_MAX_AGE_US)
        out.irradiance = s->irradiance;
    const bool fresh = out.temp_age_ms != ENV_NO_AGE && now - s->temp_acquired_us <= ENV_MAX_AGE_US;
    double sum = 0; unsigned count = 0;
    for (unsigned i = 0; i < 4; ++i) {
        out.temp_errors[i] = s->temp_errors[i];
        out.temperatures[i] = fresh && (s->temp_mask & (1U << i)) ? s->temperatures[i] : NAN;
        if (isfinite(out.temperatures[i])) {
            sum += out.temperatures[i]; ++count; out.temp_mask |= 1U << i;
        }
    }
    if (count) out.temperature = (float)(sum / count);
    return out;
}

uint16_t environment_modbus_crc(const uint8_t *data, size_t size)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1U) ? 0xA001U : 0);
    }
    return crc;
}

bool environment_decode_irradiance(const uint8_t *f, size_t size, uint16_t *value)
{
    if (!f || !value || size != 7 || f[0] != 1 || f[1] != 3 || f[2] != 2 ||
        environment_modbus_crc(f, 5) != ((uint16_t)f[5] | ((uint16_t)f[6] << 8))) return false;
    *value = ((uint16_t)f[3] << 8) | f[4];
    return true;
}

static void number(char *buffer, size_t size, float value)
{
    if (isfinite(value)) snprintf(buffer, size, "%.2f", (double)value);
    else snprintf(buffer, size, "null");
}

size_t environment_format_json(char *buffer, size_t size, const environment_snapshot_t *s)
{
    char irr[20], mean[20], temp[4][20], ia[16], ta[16];
    number(irr, sizeof(irr), s->irradiance); number(mean, sizeof(mean), s->temperature);
    for (unsigned i = 0; i < 4; ++i) number(temp[i], sizeof(temp[i]), s->temperatures[i]);
    if (s->irr_age_ms == ENV_NO_AGE) snprintf(ia, sizeof(ia), "null");
    else snprintf(ia, sizeof(ia), "%" PRIu32, s->irr_age_ms);
    if (s->temp_age_ms == ENV_NO_AGE) snprintf(ta, sizeof(ta), "null");
    else snprintf(ta, sizeof(ta), "%" PRIu32, s->temp_age_ms);
    // Claves cortas del detalle ambiental para conservar 10 Hz a 115200 baud.
    const int length = snprintf(buffer, size,
        "\"irr\":%s,\"temp\":%s,\"env\":{\"us\":\"%" PRIu64 "\",\"t\":[%s,%s,%s,%s],"
        "\"mask\":%u,\"ia\":%s,\"ta\":%s,\"ie\":%d,\"te\":[%d,%d,%d,%d]}",
        irr, mean, s->snapshot_us, temp[0], temp[1], temp[2], temp[3], (unsigned)s->temp_mask,
        ia, ta, s->irr_error, s->temp_errors[0], s->temp_errors[1], s->temp_errors[2], s->temp_errors[3]);
    if (length < 0 || (size_t)length >= size) { if (size) buffer[0] = 0; return 0; }
    return (size_t)length;
}

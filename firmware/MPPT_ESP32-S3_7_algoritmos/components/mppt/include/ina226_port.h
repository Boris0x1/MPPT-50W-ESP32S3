#ifndef INA226_PORT_H
#define INA226_PORT_H
#include <stdint.h>
/* Callbacks transfer one 16-bit register in host byte order; implementation
 * must serialize the I2C wire value MSB first. Return zero on success. */
typedef int (*ina_read16_t)(void *ctx,uint8_t reg,uint16_t *value);
typedef int (*ina_write16_t)(void *ctx,uint8_t reg,uint16_t value);
typedef struct {
    void *ctx; ina_read16_t read; ina_write16_t write;
    double current_lsb_A; uint16_t calibration;
    int initialized;
} ina226_t;
/* Model nominal shunt=.01 ohm and requested current LSB=.00025 A.
 * Measure actual shunt and calibrate before use. Does not assign GPIO/address. */
int ina226_init(ina226_t *s,void *ctx,ina_read16_t read,ina_write16_t write,
                double shunt_ohm,double requested_lsb_A);
int ina226_trigger(ina226_t *s);
/* 0=ready, 1=not ready, -1=I2C/config error, -2=math overflow. */
int ina226_read_pair(ina226_t *s,double *Vpv,double *Ipv);
#endif

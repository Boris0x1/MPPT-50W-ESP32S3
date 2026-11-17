#pragma once

// INA226 de entrada. A0 y A1 a GND corresponden a la direccion 0x40.
#define INA226_I2C_ADDRESS       0x40
#define INA226_SDA_GPIO          14
#define INA226_SCL_GPIO          13
#define INA226_ALERT_GPIO        12
#define INA226_I2C_SPEED_HZ      400000
#define INA226_SHUNT_OHMS        0.01   // R010, confirmado para este modulo.
#define INA226_I2C_TIMEOUT_MS    20
#define INA226_READY_TIMEOUT_MS  60

#define PWM_GPIO                4
#define PWM_FREQ_HZ             75000U
#define PWM_TIMER_RESOLUTION_HZ 160000000U
#define PWM_PERIOD_TICKS        ((PWM_TIMER_RESOLUTION_HZ + PWM_FREQ_HZ / 2U) / PWM_FREQ_HZ)
// Conservar el limite fisico original 716/1024, ahora en ticks nativos.
#define PWM_MAX_COMPARE_TICKS   ((PWM_PERIOD_TICKS * 716U) / 1024U)

// Consulta del INA226 con PWM detenido; no es un algoritmo MPPT.
#define STOP_ACQUISITION_PERIOD_MS 40

// La web recibe snapshots, independientes de las actualizaciones del control.
#define TELEMETRY_PERIOD_US     100000ULL
#define FAST_PERIOD_US          2000U
#define FAST_MAX_LATENESS_US    500U

// Ambiente: mismo protocolo/sensores del H2, adquisicion fuera del lazo MPPT.
#define IRR_UART_TX_GPIO        39
#define IRR_UART_RX_GPIO        40
#define IRR_UART_BAUD_RATE      4800
#define TEMPERATURE_T0_GPIO     41
#define TEMPERATURE_T1_GPIO     42
#define TEMPERATURE_T2_GPIO     2
#define TEMPERATURE_T3_GPIO     1
#define ENV_PERIOD_MS           1000U
#define ENV_MAX_AGE_US          5000000ULL

// Mismo circuito de cargas del C6; la rama de 10 ohm es activa en LOW.
#define LOAD_GPIO_30            21
#define LOAD_GPIO_10            47
#define LOAD_GPIO_20            38
#define LOAD_INITIAL_OHMS       10

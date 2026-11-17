#include "mosfet_pwm.h"
#include "mppt_config.h"
#include "driver/gpio.h"
#include "driver/mcpwm_prelude.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include <stdbool.h>
#include <inttypes.h>

static mcpwm_timer_handle_t timer;
static mcpwm_oper_handle_t oper;
static mcpwm_cmpr_handle_t comparator;
static mcpwm_gen_handle_t generator;
static bool initialized, running;

// Solo usado al detener/rearmar, nunca en las actualizaciones normales de duty.
// STOP_EMPTY es asincrono: dejar transcurrir un periodo garantiza contador parado
// en cero antes de retirar la fuerza LOW en el siguiente arranque.
#define STOP_WAIT_US ((PWM_PERIOD_TICKS * 1000000ULL + PWM_TIMER_RESOLUTION_HZ - 1U) / PWM_TIMER_RESOLUTION_HZ + 1U)
_Static_assert(PWM_PERIOD_TICKS > 1 && PWM_PERIOD_TICKS < 65535, "Periodo MCPWM fuera de rango");
_Static_assert(PWM_MAX_COMPARE_TICKS < PWM_PERIOD_TICKS, "Duty maximo fuera de rango");

static esp_err_t fail_low(esp_err_t error)
{
    if (generator) (void)mcpwm_generator_set_force_level(generator, 0, true);
    else (void)gpio_set_level(PWM_GPIO, 0);
    return error;
}
#define PWM_TRY(call) do { const esp_err_t err_ = (call); if (err_ != ESP_OK) return fail_low(err_); } while (0)

esp_err_t mosfet_pwm_set_count(uint32_t count)
{
    if (!initialized) return ESP_ERR_INVALID_STATE;
    if (count > PWM_MAX_COMPARE_TICKS) return fail_low(ESP_ERR_INVALID_ARG);
    if (!count) {
        PWM_TRY(mcpwm_generator_set_force_level(generator, 0, true));
        if (running) {
            PWM_TRY(mcpwm_timer_start_stop(timer, MCPWM_TIMER_STOP_EMPTY));
            esp_rom_delay_us((uint32_t)STOP_WAIT_US);
            running = false;
        }
        return ESP_OK;
    }
    // El shadow se transfiere en TEZ, evitando modificar el pulso en curso.
    PWM_TRY(mcpwm_comparator_set_compare_value(comparator, count));
    if (!running) {
        // Con el contador detenido, preparar LOW de un solo evento antes de
        // quitar la fuerza continua. Solo TEZ podra volver a elevar la salida.
        PWM_TRY(mcpwm_generator_set_force_level(generator, 0, false));
        PWM_TRY(mcpwm_generator_set_force_level(generator, -1, true));
        PWM_TRY(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
        running = true;
    }
    return ESP_OK;
}

esp_err_t mosfet_pwm_init(void)
{
    if (initialized) return ESP_OK;
    PWM_TRY(gpio_set_direction(PWM_GPIO, GPIO_MODE_OUTPUT));
    PWM_TRY(gpio_set_level(PWM_GPIO, 0));
    const mcpwm_timer_config_t timer_config = {
        .group_id = 0, .clk_src = MCPWM_TIMER_CLK_SRC_PLL160M,
        .resolution_hz = PWM_TIMER_RESOLUTION_HZ,
        .period_ticks = PWM_PERIOD_TICKS, .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    PWM_TRY(mcpwm_new_timer(&timer_config, &timer));
    const mcpwm_operator_config_t operator_config = {.group_id = 0};
    PWM_TRY(mcpwm_new_operator(&operator_config, &oper));
    PWM_TRY(mcpwm_operator_connect_timer(oper, timer));
    const mcpwm_comparator_config_t comparator_config = {.flags.update_cmp_on_tez = true};
    PWM_TRY(mcpwm_new_comparator(oper, &comparator_config, &comparator));
    const mcpwm_generator_config_t generator_config = {.gen_gpio_num = PWM_GPIO};
    PWM_TRY(mcpwm_new_generator(oper, &generator_config, &generator));
    PWM_TRY(mcpwm_generator_set_force_level(generator, 0, true));
    PWM_TRY(mcpwm_generator_set_action_on_timer_event(generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    PWM_TRY(mcpwm_generator_set_action_on_compare_event(generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));
    PWM_TRY(mcpwm_comparator_set_compare_value(comparator, 1));
    PWM_TRY(mcpwm_timer_enable(timer));
    // Sin ISR de PWM. El contador arranca solo al recibir un duty no nulo.
    initialized = true;
    ESP_LOGI("MCPWM", "GPIO%d: solicitado=%u Hz; reloj=%u Hz; periodo=%u ticks; real=%.3f Hz; paso=%.6f%%",
             PWM_GPIO, PWM_FREQ_HZ, PWM_TIMER_RESOLUTION_HZ, PWM_PERIOD_TICKS,
             (double)PWM_TIMER_RESOLUTION_HZ / PWM_PERIOD_TICKS, 100.0 / PWM_PERIOD_TICKS);
    return ESP_OK;
}

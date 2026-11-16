#ifndef MODEL_OUTPUT_H
#define MODEL_OUTPUT_H
#include <stdbool.h>
#include <stdint.h>
/* Common downstream damping from the Simulink model, separate from MPPT.
 * It uses the same sampled Vpv. It is not a new MPPT algorithm. */
typedef struct { double filtered_V; bool initialized; } model_output_t;
void model_output_init(model_output_t *s);
double model_output_update(model_output_t *s,double Vpv,double Dbase);
uint16_t model_duty_count(double Dstage);
#endif

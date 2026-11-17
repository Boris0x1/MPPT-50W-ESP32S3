/*
 * Academic License - for use in teaching, academic research, and meeting
 * course requirements at degree granting institutions only. Not for
 * government, commercial, or other organizational use.
 *
 * Adaptacion de quietWindow, mppt_unificado_50w.c (MATLAB Coder 24.2).
 * Solo se sustituye la aritmetica de indices de su anillo de 32 muestras.
 * Sumas, orden, precision double, umbrales y decisiones son los originales.
 * Los archivos entregados en components/mppt permanecen intactos.
 */
#include "mppt_unificado_50w.h"
#include "minOrMax.h"
#include "rt_nonfinite.h"
#include <math.h>

// GNU ld --wrap mantiene la funcion original para el dominio no optimizado.
boolean_T __real_quietWindow(const double v[32], const double p[32], const double d[32],
                            double next, double *V, double *P, double *D);

boolean_T __wrap_quietWindow(const double v[32], const double p[32], const double d[32],
                            double next, double *V, double *P, double *D)
{
    // ringNext siempre es un entero de 1 a 32 en el controlador. Comprobar
    // antes de convertir evita conversiones indefinidas en esta ruta rapida.
    if (!(next >= 1.0 && next <= 32.0))
        return __real_quietWindow(v, p, d, next, V, P, D);
    const unsigned first = (unsigned)next;
    if (next != (double)first)
        return __real_quietWindow(v, p, d, next, V, P, D);

    double sumOldV = 0.0, sumOldP = 0.0, sumV = 0.0, sumP = 0.0, sumD = 0.0;
    for (unsigned k = 0; k < 16; ++k) {
        // Para indices enteros no negativos, x % 32 equivale a x & 31.
        // No hay fmod, ni sumas/conversiones double para indexar cada muestra.
        const unsigned earlier = (first - 1U + k) & 31U;
        const unsigned later = (first + 15U + k) & 31U;
        sumOldV += v[earlier];
        sumOldP += p[earlier];
        sumV += v[later];
        sumP += p[later];
        sumD += d[later];
    }
    *V = sumV / 16.0;
    *P = sumP / 16.0;
    *D = sumD / 16.0;
    sumV = 0.011 * *P;
    if ((sumV <= 0.09) || rtIsNaN(sumV)) sumV = 0.09;
    if (fabs(*P - sumOldP / 16.0) <= sumV) {
        sumV = 0.011 * *V;
        if ((sumV <= 0.07) || rtIsNaN(sumV)) sumV = 0.07;
        if ((fabs(*V - sumOldV / 16.0) <= sumV) &&
            (maximum(d) - minimum(d) <= 0.016) &&
            (maximum(v) - minimum(v) <= 0.8)) return true;
    }
    return false;
}

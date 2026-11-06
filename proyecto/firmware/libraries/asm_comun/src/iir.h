// Nodo 2 temporal (Santiago): IIR en secciones de segundo orden, float32.
// y[n] = b0 x[n] + b1 x[n-1] + b2 x[n-2] - a1 y[n-1] - a2 y[n-2]  (a0 = 1)
#pragma once
#include "diagnostico.h"
#ifdef __cplusplus
extern "C" {
#endif

#define IIR_MAX_ETAPAS 6

typedef struct {
    float b0, b1, b2, a1, a2;
    float z1, z2;            // estado (forma directa II transpuesta)
} biquad_t;

typedef struct {
    biquad_t etapas[IIR_MAX_ETAPAS];
    int n_etapas;
    float ganancia;
} iir_t;

void iir_reiniciar_estado(iir_t *f);
// Elige y calcula los coeficientes a partir del diagnostico del nodo 1
// (p. ej. notch centrado en frecuencia_hz). Debe verificar estabilidad: |polos| < 1.
int iir_disenar(iir_t *f, const diagnostico_t *d, float fs);
// Cargar coeficientes calculados afuera (p. ej. el eliptico de la Tarea 1)
int iir_cargar_sos(iir_t *f, const float *sos, int n_etapas);
int iir_procesar(iir_t *f, const float *x, float *y, int n);

#ifdef __cplusplus
}
#endif

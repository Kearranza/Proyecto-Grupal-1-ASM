// Nodo 2 temporal (Santiago). Base: src/filtros_temporales.py de la Tarea 1
// (filtrar_sos_manual). Validar con pruebas_c/test_iir.c contra scipy.
#include <string.h>
#include "config.h"
#include "iir.h"

void iir_reiniciar_estado(iir_t *f) {
    for (int i = 0; i < f->n_etapas; i++) {
        f->etapas[i].z1 = 0.0f;
        f->etapas[i].z2 = 0.0f;
    }
}

int iir_disenar(iir_t *f, const diagnostico_t *d, float fs) {
    (void)f; (void)d; (void)fs;
    return ASM_PENDIENTE;  // TODO (Santiago): coeficientes segun d->tipo y d->frecuencia_hz
}

int iir_cargar_sos(iir_t *f, const float *sos, int n_etapas) {
    // sos en el formato de scipy: por etapa [b0 b1 b2 a0 a1 a2], con a0 = 1
    (void)f; (void)sos; (void)n_etapas;
    return ASM_PENDIENTE;  // TODO (Santiago)
}

int iir_procesar(iir_t *f, const float *x, float *y, int n) {
    (void)f; (void)x; (void)y; (void)n;
    return ASM_PENDIENTE;  // TODO (Santiago): DF2T por etapa
}

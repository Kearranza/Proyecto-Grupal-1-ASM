// Nodo 3 espectral (Gretchell). Base: src/filtrado_espectral.py de la Tarea 1
// (reducir_ruido, conservar_simetria). Usa fft() e ifft() de fft.c.
#include <string.h>
#include "espectral.h"
#include "fft.h"

int espectral_iniciar(espectral_t *e, int n) {
    if (!n_es_valido(n)) return ASM_ERROR;
    memset(e, 0, sizeof(*e));
    e->n = n;
    return ASM_OK;
}

int espectral_procesar_salto(espectral_t *e, const float *nuevas, float *salida,
                             const diagnostico_t *d, float fs) {
    (void)e; (void)nuevas; (void)salida; (void)d; (void)fs;
    return ASM_PENDIENTE;  // TODO (Gretchell)
}

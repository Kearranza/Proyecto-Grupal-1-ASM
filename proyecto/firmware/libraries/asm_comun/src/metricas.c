// Metricas (Gretchell). Validar contra src/metricas.py con pruebas_c/test_metricas.c.
#include <string.h>
#include "config.h"
#include "metricas.h"

void metricas_reiniciar(metricas_t *m) {
    memset(m, 0, sizeof(*m));
}

int metricas_acumular(metricas_t *m, const float *s, const float *x, const float *y, int n) {
    (void)m; (void)s; (void)x; (void)y; (void)n;
    return ASM_PENDIENTE;  // TODO (Gretchell)
}

int metricas_calcular(const metricas_t *m, resultado_metricas_t *r) {
    (void)m; (void)r;
    return ASM_PENDIENTE;  // TODO (Gretchell)
}

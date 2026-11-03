// Nodo 1 (Luis). Portar la logica de clasificar_ruido() de src/filtrado_espectral.py
// (Tarea 1): planitud espectral para separar tonal de banda ancha, pico dominante
// fuera de las bandas utiles, etc. Validar contra Python con pruebas_c/test_diagnostico.c.
#include "config.h"
#include "diagnostico.h"

int diagnostico_estimar(const float *magnitud, int n, float fs, diagnostico_t *salida) {
    (void)magnitud; (void)n; (void)fs; (void)salida;
    return ASM_PENDIENTE;  // TODO (Luis)
}

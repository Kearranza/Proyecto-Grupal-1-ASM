// Casos de frontera de integracion y compromiso entre ruido y senal util.
#include "prueba.h"
#include "nodo3.h"
#include <string.h>

static nodo3_t p, copia;
static float x[SALTO_MAX], s[SALTO_MAX], y[SALTO_MAX];

static void probar_rechazo_atomico(void) {
    diagnostico_t d = {RUIDO_NINGUNO, 0, 512, 0, 0, 0};
    bandas_nodo3_t b = {1, 500, 400, 3000, 4000};
    nodo3_iniciar(&p, 512, 1, 0);
    nodo3_generar_demo(0, 256, x, s);
    copia = p;
    int bien = nodo3_procesar(&p, x, s, y, &d, &b) == ASM_ERROR;
    bien &= memcmp(&p, &copia, sizeof p) == 0;
    s[100] = NAN;
    bien &= nodo3_procesar(&p, x, s, y, &d, NULL) == ASM_ERROR;
    bien &= memcmp(&p, &copia, sizeof p) == 0;
    s[100] = 0;
    d.n_bloque = 1024;
    bien &= nodo3_procesar(&p, x, s, y, &d, NULL) == ASM_ERROR;
    bien &= memcmp(&p, &copia, sizeof p) == 0;
    if (bien) OK("bandas, referencia y diagnostico invalidos no consumen el salto");
    else FALLA("rechazo atomico", "estado alterado");
}

static void probar_referencia_intermitente(void) {
    diagnostico_t d = {RUIDO_NINGUNO, 0, 512, 0, 0, 0};
    nodo3_iniciar(&p, 512, 0, 0);
    int bien = 1;
    for (int i = 0; i < 4; i++) {
        nodo3_generar_demo(i * 256, 256, x, s);
        bien &= nodo3_procesar(&p, x, i == 1 ? NULL : s, y, &d, NULL) == ASM_OK;
        // Se evalua el salto anterior, no la referencia recien recibida.
        long esperado[] = {0, 256, 256, 512};
        bien &= p.metricas.muestras == esperado[i];
    }
    if (bien) OK("referencia intermitente respeta el retraso de un salto");
    else FALLA("referencia intermitente", "muestras mal alineadas");
}

static void probar_superposicion(void) {
    diagnostico_t d = {RUIDO_TONAL, 0, 1024, 3500, 0, 0};
    bandas_nodo3_t b = {1, 437.5f, 3562.5f, 3437.5f, 3562.5f};
    nodo3_iniciar(&p, 1024, 1, 0);
    int bien = 1;
    for (int bloque = 0; bloque < 32; bloque++) {
        for (int i = 0; i < 512; i++) {
            double t = (bloque * 512 + i) / (double)FS_HZ;
            // La interferencia comparte una frecuencia con la referencia limpia.
            s[i] = (float)(0.3 * sin(2 * M_PI * 500 * t) + 0.3 * sin(2 * M_PI * 3500 * t));
            x[i] = s[i] + (float)(0.3 * sin(2 * M_PI * 3500 * t));
        }
        bien &= nodo3_procesar(&p, x, s, y, &d, &b) == ASM_OK;
    }
    resultado_metricas_t r = {0};
    bien &= metricas_calcular(&p.metricas, &r) == ASM_OK;
    if (bien && r.energia_util_pct > 45 && r.energia_util_pct < 55 && r.atenuacion_interferencia_db > 20 && fabsf(r.delta_snr_db) < 1)
        OK("superposicion: rechazo alto no implica conservar la senal util");
    else FALLA("superposicion", "energia %.2f, atenuacion %.2f, mejora %.2f", r.energia_util_pct, r.atenuacion_interferencia_db, r.delta_snr_db);
    printf("              energia=%.3f %% atenuacion=%.3f dB mejora=%.3f dB\n", r.energia_util_pct, r.atenuacion_interferencia_db, r.delta_snr_db);
}

int main(void) {
    printf("\n== regresion Nodo 3 ==\n");
    probar_rechazo_atomico();
    probar_referencia_intermitente();
    probar_superposicion();
    return resumen("regresion Nodo 3");
}

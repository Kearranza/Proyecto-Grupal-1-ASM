// Nodo 2 temporal (Santiago). Base: src/filtros_temporales.py de la Tarea 1
// (filtrar_sos_manual). Validar con pruebas_c/test_iir.c contra scipy.
#include <string.h>
#include "config.h"
#include "iir.h"


#define F_RED_MAX_HZ    100.0f    // por debajo, se asume red electrica: se anulan armonicos
#define ARMONICOS_MAX     5
#define FRAC_NYQUIST      0.45    // no se disenan notches por encima de 0,45 fs
#define F_MIN_HZ          5.0f


void iir_reiniciar_estado(iir_t *f) {
    for (int i = 0; i < f->n_etapas; i++) {
        f->etapas[i].z1 = 0.0f;
        f->etapas[i].z2 = 0.0f;
    }
}


int iir_disenar(iir_t *f, const diagnostico_t *d, float fs) {
    if (!f || !d || !(fs > 0.0f)) return ASM_ERROR;
    const int n_bloque = d->n_bloque > 0 ? d->n_bloque : N_POR_DEFECTO;

    iir_t nuevo;
    memset(&nuevo, 0, sizeof nuevo);
    nuevo.ganancia = 1.0f;

    switch (d->tipo) {
    case RUIDO_TONAL: {

        const float f0 = d->frecuencia_hz;
        if (!(f0 >= F_MIN_HZ && f0 < FRAC_NYQUIST * fs)) return ASM_ERROR;
        const int armonicos = (f0 < F_RED_MAX_HZ) ? ARMONICOS_MAX : 1;
        for (int k = 1; k <= armonicos && nuevo.n_etapas < IIR_MAX_ETAPAS; k++) {
            const double fk = (double)k * f0;
            if (fk >= FRAC_NYQUIST * fs) break;
            disenar_notch(&nuevo.etapas[nuevo.n_etapas++], fk,
                          ancho_para_tolerancia(f0, fs, n_bloque), fs);
        }
        break;
    }
    case RUIDO_BANDA: {

        const float f0 = d->frecuencia_hz, bw = d->ancho_hz;
        if (!(f0 >= F_MIN_HZ && f0 < FRAC_NYQUIST * fs && bw > 0.0f)) return ASM_ERROR;
        const double ancho = fmin((double)bw, (double)f0);   
        for (int k = 0; k < 2; k++)
            disenar_notch(&nuevo.etapas[nuevo.n_etapas++], f0, ancho, fs);
        break;
    }
    case RUIDO_NINGUNO:
    case RUIDO_BLANCO:
    case RUIDO_IMPULSIVO:
        // Un filtro temporal fijo no ayuda con estos ruidos: pasa directo y deja el trabajo al nodo 3 (espectral).
        filtro_identidad(&nuevo);
        break;
    default:
        return ASM_ERROR;
    }

    if (!todas_estables(&nuevo)) return ASM_ERROR;
    *f = nuevo;
    return ASM_OK;
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

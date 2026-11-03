// Nodo 1 (Luis). Mejora clasificar_ruido() de src/filtrado_espectral.py (Tarea 1), que
// comparaba el pico con la mediana de TODO el espectro: en caso_3 eso da 101 dB y lo
// llamaba tonal, y en caso_1 tomaba el tono util de 440 Hz como interferencia.
// Aqui todo se mide FUERA de las bandas utiles (DC y cada tono util +-DIAG_MARGEN_BINS):
//   1. potencia de afuera / potencia util < DIAG_UMBRAL_NINGUNO_DB    -> ninguno
//   2. planitud espectral > DIAG_PLANITUD_BLANCO                      -> blanco
//   3. potencia en el lobulo del pico > DIAG_CONCENTRACION_TONAL      -> tonal
//   4. si no                                                          -> banda
// Los umbrales vienen de config/sistema.json. En float y no double: la Pico W no
// tiene FPU y esto corre en cada bloque. Validar con pruebas_c/test_diagnostico.c.
#include <math.h>
#include <stdlib.h>
#include "config.h"
#include "diagnostico.h"

#define LOBULO_HANN 2      // el lobulo principal de la ventana Hann mide +-2 bins
#define MINIMO 1e-20f      // evita log(0) y divisiones entre 0

static float utiles_hz[DIAG_UTILES_MAX] = DIAG_FRECUENCIAS_UTILES_HZ;
static int n_utiles = DIAG_N_UTILES;

int diagnostico_fijar_utiles(const float *frecuencias_hz, int cantidad) {
    if (cantidad < 0 || cantidad > DIAG_UTILES_MAX || (cantidad > 0 && !frecuencias_hz))
        return ASM_ERROR;
    for (int i = 0; i < cantidad; i++) utiles_hz[i] = frecuencias_hz[i];
    n_utiles = cantidad;
    return ASM_OK;
}

// 1 si el bin k cae en el lobulo de DC o en el de algun tono util
static int es_util(int k, float df) {
    if (k <= DIAG_MARGEN_BINS) return 1;
    for (int i = 0; i < n_utiles; i++)
        if (abs(k - (int)lroundf(utiles_hz[i] / df)) <= DIAG_MARGEN_BINS) return 1;
    return 0;
}

// Fraccion de bin que hay que sumarle a k: vertice de la parabola que pasa por
// log|X| en k-1, k y k+1. Un tono de 3510 Hz no cae en un bin con N = 1024.
static float ajuste_de_bin(const float *m, int k, int ultimo) {
    if (k <= 0 || k >= ultimo) return 0.0f;
    float a = logf(fmaxf(m[k - 1], MINIMO));
    float b = logf(fmaxf(m[k], MINIMO));
    float c = logf(fmaxf(m[k + 1], MINIMO));
    float curvatura = a - 2.0f * b + c;
    return curvatura < 0.0f ? 0.5f * (a - c) / curvatura : 0.0f;
}

int diagnostico_estimar(const float *magnitud, int n, float fs, diagnostico_t *salida) {
    if (!magnitud || !salida || !n_es_valido(n) || fs <= 0.0f) return ASM_ERROR;
    const float df = fs / n;
    const int ultimo = n / 2;

    // Una pasada: potencia util y de afuera, sumas para la planitud y pico de afuera
    float p_util = 0.0f, p_fuera = 0.0f, suma_mag = 0.0f, suma_log = 0.0f;
    int cuenta = 0, pico = -1;
    for (int k = 0; k <= ultimo; k++) {
        float p = magnitud[k] * magnitud[k];
        if (es_util(k, df)) { p_util += p; continue; }
        p_fuera += p;
        suma_mag += magnitud[k];
        suma_log += logf(fmaxf(magnitud[k], MINIMO));
        cuenta++;
        if (pico < 0 || magnitud[k] > magnitud[pico]) pico = k;
    }
    if (cuenta == 0) return ASM_ERROR;   // las bandas utiles tapan todo el espectro

    salida->tipo = RUIDO_NINGUNO;
    salida->reservado = 0;
    salida->n_bloque = (uint16_t)n;
    salida->frecuencia_hz = 0.0f;
    salida->ancho_hz = 0.0f;
    salida->nivel_db = 10.0f * log10f((p_fuera + MINIMO) / (p_util + MINIMO));
    if (p_fuera <= 0.0f || salida->nivel_db < DIAG_UMBRAL_NINGUNO_DB) return ASM_OK;

    // Media geometrica / media aritmetica. Con ruido blanco |X[k]| sigue una Rayleigh
    // y la planitud tiende a 0.85; con ruido angosto casi todos los bins son ~0
    float planitud = expf(suma_log / cuenta) / (suma_mag / cuenta);
    if (planitud > DIAG_PLANITUD_BLANCO) {
        salida->tipo = RUIDO_BLANCO;
        salida->ancho_hz = fs / 2.0f;
        return ASM_OK;
    }

    // Un tono mete casi toda su potencia en el lobulo principal; una banda, no
    float p_lobulo = 0.0f;
    for (int k = pico - LOBULO_HANN; k <= pico + LOBULO_HANN; k++)
        if (k >= 0 && k <= ultimo && !es_util(k, df)) p_lobulo += magnitud[k] * magnitud[k];
    if (p_lobulo / p_fuera > DIAG_CONCENTRACION_TONAL) {
        salida->tipo = RUIDO_TONAL;
        salida->frecuencia_hz = (pico + ajuste_de_bin(magnitud, pico, ultimo)) * df;
        return ASM_OK;
    }

    // Banda: bins de afuera a menos de DIAG_CAIDA_BANDA_DB del pico. Centro = centroide
    // de potencia; ancho = sqrt(12) * desviacion, que es el ancho de una banda plana
    const float umbral = magnitud[pico] * powf(10.0f, -DIAG_CAIDA_BANDA_DB / 20.0f);
    float w = 0.0f, wk = 0.0f, wk2 = 0.0f;
    for (int k = 0; k <= ultimo; k++) {
        if (es_util(k, df) || magnitud[k] < umbral) continue;
        float p = magnitud[k] * magnitud[k];
        w += p;
        wk += p * k;
        wk2 += p * k * k;
    }
    float centro = wk / w;
    salida->tipo = RUIDO_BANDA;
    salida->frecuencia_hz = centro * df;
    salida->ancho_hz = sqrtf(12.0f * fmaxf(wk2 / w - centro * centro, 0.0f)) * df;
    return ASM_OK;
}

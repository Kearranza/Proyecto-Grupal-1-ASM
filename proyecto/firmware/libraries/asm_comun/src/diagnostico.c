// Nodo 1 (Luis): diagnostico del ruido a partir de un bloque.
//
// Arreglo respecto a clasificar_ruido() de la Tarea 1: primero se EXCLUYEN las bandas
// de la senal util. Sin eso, el tono util de 440 Hz es el pico mas alto y parece la
// interferencia, y en caso_3 los dos tonos dentro de la banda la hacen ver tonal.
//
// Sobre los bins que quedan (los "de ruido"):
//   1. nivel_db = energia de ruido / energia util. Si es muy bajo -> NINGUNO.
//   2. contraste del pico: su potencia contra la mediana de los bins vecinos (3 a 16
//      bins a cada lado; se toma el lado mas alto para no confundir el borde de una
//      banda con un tono). Un tono con Hann deja los vecinos 30 dB o mas abajo;
//      el ruido de banda o blanco, no. Alto -> TONAL.
//   3. planitud espectral (media geometrica / aritmetica). Cerca de 1 -> BLANCO.
//   4. si no, BANDA: la que contiene del 5 % al 95 % de la energia de ruido.
// diagnostico_estimar_bloque() agrega IMPULSIVO mirando la senal en el tiempo.
//
// Umbrales elegidos con los casos de la Tarea 1 y senales sinteticas con ADC de 12
// bits (ver comentarios). Con N = 256 la resolucion (62,5 Hz) no alcanza para separar
// ruido de banda angosto de un tono: es una limitacion, no un error.
#include <math.h>
#include <string.h>
#include "config.h"
#include "diagnostico.h"

// Ruido mas de 30 dB por debajo de la senal util: no vale la pena filtrar.
// (Solo tonos con ADC de 12 bits dio -50 dB; con ruido a 10 dB de SNR, -10 dB.)
#define NIVEL_MIN_DB -30.0f
// Tono puro: 80 a 98 dB. Ruido blanco y de banda: 4 a 13 dB con N >= 512.
#define CONTRASTE_TONAL_DB 20.0f
// Blanco: 0,84. Banda: 0,001 a 0,01. Cualquier valor intermedio separa.
#define PLANITUD_BLANCO 0.5f
// Curtosis de la primera diferencia: ~3 en ruido gaussiano, ~2 con tonos,
// 12 a 50 con impulsos. 4,5 deja margen sobre la variacion del blanco (max 3,5).
#define CURTOSIS_IMPULSIVO 4.5f
#define VECINOS_MIN 3      // fuera del lobulo principal de Hann (+-2 bins)
#define VECINOS_MAX 16
#define K_MIN 2            // bins 0 y 1: DC y su fuga, ya se quito la media

static float utiles[DIAG_MAX_UTILES] = DIAG_UTILES_HZ;
static int n_utiles = DIAG_N_UTILES;
static float ancho_util = DIAG_ANCHO_UTIL_HZ;

int diagnostico_configurar_utiles(const float *centros_hz, int cantidad, float ancho_hz) {
    if (cantidad < 0 || cantidad > DIAG_MAX_UTILES || !(ancho_hz >= 0.0f)) return ASM_ERROR;
    for (int i = 0; i < cantidad; i++) utiles[i] = centros_hz[i];
    n_utiles = cantidad;
    ancho_util = ancho_hz;
    return ASM_OK;
}

// El ancho excluido nunca es menor que el lobulo principal de Hann (+-2 bins) mas
// medio bin: con N chico, un tono util "derrama" fuera de +-60 Hz.
static int es_util(int k, float df) {
    float f = k * df, ancho = fmaxf(ancho_util, 2.5f * df);
    for (int i = 0; i < n_utiles; i++)
        if (fabsf(f - utiles[i]) <= ancho) return 1;
    return 0;
}

static int es_ruido(int k, float df) {
    return k >= K_MIN && !es_util(k, df);
}

static float mediana(float *v, int n) {
    for (int i = 1; i < n; i++) {           // insercion: n <= 14
        float x = v[i];
        int j = i - 1;
        for (; j >= 0 && v[j] > x; j--) v[j + 1] = v[j];
        v[j + 1] = x;
    }
    return n % 2 ? v[n / 2] : 0.5f * (v[n / 2 - 1] + v[n / 2]);
}

// Mediana de la potencia de los vecinos de ruido a un lado del pico (lado = -1 o +1)
static float fondo_lado(const float *magnitud, int n, float df, int kp, int lado) {
    float v[VECINOS_MAX - VECINOS_MIN + 1];
    int cuantos = 0;
    for (int d = VECINOS_MIN; d <= VECINOS_MAX; d++) {
        int k = kp + lado * d;
        if (k < 0 || k > n / 2 || !es_ruido(k, df)) continue;
        v[cuantos++] = magnitud[k] * magnitud[k];
    }
    return cuantos >= 4 ? mediana(v, cuantos) : 0.0f;
}

int diagnostico_estimar(const float *magnitud, int n, float fs, diagnostico_t *d) {
    if (!n_es_valido(n) || fs <= 0.0f || !magnitud || !d) return ASM_ERROR;
    const float df = fs / n;
    const int mitad = n / 2;
    memset(d, 0, sizeof *d);
    d->n_bloque = (uint16_t)n;

    // Energias, pico y media de log(magnitud) sobre los bins de ruido.
    // En float: el RP2040 no tiene FPU y double cuesta varias veces mas.
    float e_util = 0.0f, e_ruido = 0.0f, suma_mag = 0.0f, suma_log = 0.0f;
    int bins_ruido = 0, kp = -1;
    for (int k = 0; k <= mitad; k++) {
        float e = magnitud[k] * magnitud[k];
        if (es_util(k, df)) { e_util += e; continue; }
        if (k < K_MIN) continue;
        e_ruido += e;
        suma_mag += magnitud[k];
        suma_log += logf(fmaxf(magnitud[k], 1e-12f));
        bins_ruido++;
        if (kp < 0 || magnitud[k] > magnitud[kp]) kp = k;
    }
    if (bins_ruido == 0) return ASM_ERROR;   // las bandas utiles cubren todo

    d->nivel_db = 10.0f * log10f((e_ruido + 1e-30f) / (e_util + 1e-30f));
    if (d->nivel_db < NIVEL_MIN_DB) {
        d->tipo = RUIDO_NINGUNO;
        return ASM_OK;
    }

    // 2. Contraste del pico contra el lado mas alto de sus vecinos
    float fondo = fmaxf(fondo_lado(magnitud, n, df, kp, -1), fondo_lado(magnitud, n, df, kp, +1));
    float contraste = 10.0f * log10f((magnitud[kp] * magnitud[kp] + 1e-30f) / (fondo + 1e-30f));
    if (contraste > CONTRASTE_TONAL_DB) {
        d->tipo = RUIDO_TONAL;
        float delta = 0.0f;
        // Interpolacion parabolica en dB para afinar la frecuencia entre bins
        if (kp > 0 && kp < mitad && es_ruido(kp - 1, df) && es_ruido(kp + 1, df)) {
            float a = 20.0f * log10f(magnitud[kp - 1] + 1e-12f);
            float b = 20.0f * log10f(magnitud[kp] + 1e-12f);
            float c = 20.0f * log10f(magnitud[kp + 1] + 1e-12f);
            float den = a - 2.0f * b + c;
            if (den < 0.0f) delta = 0.5f * (a - c) / den;
        }
        d->frecuencia_hz = (kp + delta) * df;
        return ASM_OK;
    }

    // 3. Planitud: media geometrica / media aritmetica de la magnitud
    float planitud = expf(suma_log / bins_ruido) / (suma_mag / bins_ruido + 1e-30f);
    if (planitud > PLANITUD_BLANCO) {
        d->tipo = RUIDO_BLANCO;
        return ASM_OK;
    }

    // 4. Banda: del 5 % al 95 % de la energia de ruido acumulada
    float acumulada = 0.0f;
    int k_bajo = -1, k_alto = mitad;
    for (int k = 0; k <= mitad; k++) {
        if (!es_ruido(k, df)) continue;
        acumulada += magnitud[k] * magnitud[k];
        if (k_bajo < 0 && acumulada >= 0.05f * e_ruido) k_bajo = k;
        if (acumulada >= 0.95f * e_ruido) { k_alto = k; break; }
    }
    d->tipo = RUIDO_BANDA;
    d->frecuencia_hz = 0.5f * (k_bajo + k_alto) * df;
    d->ancho_hz = (k_alto - k_bajo) * df;
    return ASM_OK;
}

// Curtosis de la primera diferencia: la diferencia atenua los tonos (bajas
// frecuencias) y deja intactos los impulsos, que son pocos y enormes.
static float curtosis_diferencia(const float *x, int n) {
    float m2 = 0.0f, m4 = 0.0f;
    for (int i = 1; i < n; i++) {
        float v = x[i] - x[i - 1], v2 = v * v;
        m2 += v2;
        m4 += v2 * v2;
    }
    m2 /= (n - 1);
    m4 /= (n - 1);
    return m2 > 0.0f ? m4 / (m2 * m2) : 0.0f;
}

int diagnostico_estimar_bloque(const float *x, const float *magnitud, int n, float fs,
                               diagnostico_t *d) {
    int r = diagnostico_estimar(magnitud, n, fs, d);
    if (r != ASM_OK || !x) return r;
    // Un tono identificado se respeta: el notch del nodo 2 lo necesita
    if (d->tipo != RUIDO_TONAL && curtosis_diferencia(x, n) > CURTOSIS_IMPULSIVO) {
        d->tipo = RUIDO_IMPULSIVO;
        d->frecuencia_hz = 0.0f;
        d->ancho_hz = 0.0f;
    }
    return ASM_OK;
}

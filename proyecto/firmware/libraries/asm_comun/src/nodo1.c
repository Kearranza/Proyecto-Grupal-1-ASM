// Nodo 1 (Luis): por cada salto de n/2 muestras nuevas
//   1. desliza el bloque (50 % de solapamiento) y escala las cuentas a [-1, 1)
//   2. quita la componente DC (la media del bloque: el ADC esta centrado en 1,65 V)
//   3. ventana Hann y magnitud de k = 0 .. n/2, con una fft() compartida de n/2 puntos:
//      el bloque es real, asi que pares e impares van juntos en un solo complejo
//   4. diagnostico_estimar_bloque(): tipo de ruido, frecuencia, ancho y nivel
#include <math.h>
#include <string.h>
#include "nodo1.h"
#include "fft.h"

#define ESCALA_ADC (1.0f / (1 << (ADC_BITS - 1)))   // 2048 cuentas = 1,0

int nodo1_iniciar(nodo1_t *p, int n) {
    if (!p || !n_es_valido(n)) return ASM_ERROR;
    memset(p, 0, sizeof *p);
    p->n = n;
    // Periodica (divide entre n, no n-1): es la misma que usa el nodo 3
    for (int i = 0; i < n; i++) p->ventana[i] = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * i / n);
    for (int k = 0; k < n / 2; k++) {
        p->giro_re[k] = cosf(2.0f * (float)M_PI * k / n);
        p->giro_im[k] = -sinf(2.0f * (float)M_PI * k / n);
    }
    return ASM_OK;
}

// FFT de n puntos reales con una de m = n/2 complejos: z = pares + j impares.
// Con Z = FFT(z): pares A[k] = (Z[k] + conj Z[m-k]) / 2, impares B[k] = (Z[k] - conj Z[m-k]) / 2j,
// y X[k] = A[k] + e^(-j 2 pi k / n) B[k]. Cuesta la mitad que la FFT compleja de n puntos.
static int magnitud_real(nodo1_t *p) {
    const int m = p->n / 2;
    for (int i = 0; i < m; i++) {
        p->re[i] = p->x[2 * i] * p->ventana[2 * i];
        p->im[i] = p->x[2 * i + 1] * p->ventana[2 * i + 1];
    }
    int r = fft(p->re, p->im, m);
    if (r != ASM_OK) return r;
    p->magnitud[0] = fabsf(p->re[0] + p->im[0]);
    p->magnitud[m] = fabsf(p->re[0] - p->im[0]);
    for (int k = 1; k < m; k++) {
        float dr = p->re[k] - p->re[m - k], di = p->im[k] + p->im[m - k];
        float ar = 0.5f * (p->re[k] + p->re[m - k]), ai = 0.5f * (p->im[k] - p->im[m - k]);
        float br = 0.5f * di, bi = -0.5f * dr;
        float xr = ar + p->giro_re[k] * br - p->giro_im[k] * bi;
        float xi = ai + p->giro_re[k] * bi + p->giro_im[k] * br;
        p->magnitud[k] = sqrtf(xr * xr + xi * xi);
    }
    return ASM_OK;
}

int nodo1_deslizar(nodo1_t *p, const int16_t *nuevas) {
    if (!p || !nuevas || !n_es_valido(p->n)) return ASM_ERROR;
    const int n = p->n, salto = n / 2;
    // La mitad nueva del bloque anterior pasa a ser la mitad vieja de este
    memmove(p->bloque, p->bloque + salto, salto * sizeof(float));
    for (int i = 0; i < salto; i++) p->bloque[salto + i] = nuevas[i] * ESCALA_ADC;
    if (p->muestras_validas < n) {
        p->muestras_validas += salto;
        if (p->muestras_validas < n) return NODO1_ARRANCANDO;
    }
    return ASM_OK;
}

int nodo1_procesar_salto(nodo1_t *p, const int16_t *nuevas, diagnostico_t *d) {
    if (!d) return ASM_ERROR;
    // 1. Deslizar el bloque
    int r = nodo1_deslizar(p, nuevas);
    if (r != ASM_OK) return r;
    const int n = p->n;

    // 2. Quitar DC
    float suma = 0.0f;
    for (int i = 0; i < n; i++) suma += p->bloque[i];
    const float media = suma / n;
    for (int i = 0; i < n; i++) p->x[i] = p->bloque[i] - media;

    // 3. Ventana, FFT y magnitud
    r = magnitud_real(p);
    if (r != ASM_OK) return r;

    // 4. Diagnostico
    return diagnostico_estimar_bloque(p->x, p->magnitud, n, FS_HZ, d);
}

void nodo1_generar_sim(uint32_t primera, int cantidad, int16_t *salida) {
    static const int frecuencias[SIM_N_TONOS] = SIM_FRECUENCIAS_HZ;
    static const float amplitudes[SIM_N_TONOS] = SIM_AMPLITUDES;
    float suma_amp = 0.0f;
    for (int t = 0; t < SIM_N_TONOS; t++) suma_amp += amplitudes[t];
    // Misma escala que el emisor: si todos coinciden en su pico, la suma da SIM_NIVEL_PICO
    const float escala = SIM_NIVEL_PICO / suma_amp * ((1 << (ADC_BITS - 1)) - 1);
    for (int i = 0; i < cantidad; i++) {
        // Frecuencias enteras: la fase se repite cada FS_HZ muestras, asi el
        // argumento del seno nunca crece y float no pierde precision
        uint32_t m = (primera + (uint32_t)i) % FS_HZ;
        float x = 0.0f;
        for (int t = 0; t < SIM_N_TONOS; t++)
            x += amplitudes[t] * sinf(2.0f * (float)M_PI * (float)((frecuencias[t] * m) % FS_HZ) / FS_HZ);
        salida[i] = (int16_t)lroundf(x * escala);
    }
}

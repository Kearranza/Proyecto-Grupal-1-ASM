// Nodo 1 (Luis): por cada salto de n/2 muestras nuevas
//   1. desliza el bloque (50 % de solapamiento) y escala las cuentas a [-1, 1)
//   2. quita la componente DC (la media del bloque: el ADC esta centrado en 1,65 V)
//   3. ventana Hann, fft() compartida y magnitud de k = 0 .. n/2
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
    return ASM_OK;
}

int nodo1_procesar_salto(nodo1_t *p, const int16_t *nuevas, diagnostico_t *d) {
    if (!p || !nuevas || !d || !n_es_valido(p->n)) return ASM_ERROR;
    const int n = p->n, salto = n / 2;

    // 1. La mitad nueva del bloque anterior pasa a ser la mitad vieja de este
    memmove(p->bloque, p->bloque + salto, salto * sizeof(float));
    for (int i = 0; i < salto; i++) p->bloque[salto + i] = nuevas[i] * ESCALA_ADC;
    if (p->muestras_validas < n) {
        p->muestras_validas += salto;
        if (p->muestras_validas < n) return NODO1_ARRANCANDO;
    }

    // 2. Quitar DC
    double suma = 0.0;
    for (int i = 0; i < n; i++) suma += p->bloque[i];
    const float media = (float)(suma / n);
    for (int i = 0; i < n; i++) p->x[i] = p->bloque[i] - media;

    // 3. Ventana, FFT y magnitud
    for (int i = 0; i < n; i++) {
        p->re[i] = p->x[i] * p->ventana[i];
        p->im[i] = 0.0f;
    }
    int r = fft(p->re, p->im, n);
    if (r != ASM_OK) return r;
    for (int k = 0; k <= n / 2; k++) p->magnitud[k] = hypotf(p->re[k], p->im[k]);

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

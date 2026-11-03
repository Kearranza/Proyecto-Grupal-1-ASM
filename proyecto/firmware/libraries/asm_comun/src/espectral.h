// Nodo 3 espectral (Gretchell): reduccion por bloques con FFT/IFFT,
// ventana Hann y solapamiento-suma al 50 %.
#pragma once
#include "config.h"
#include "diagnostico.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int n;                          // tamano de bloque actual
    float umbral;                   // parametro adaptativo (ajustable sin recompilar)
    float ancho_rechazo_hz;         // parametro adaptativo
    float entrada[N_MAX];           // ultimas n muestras
    float acumulado[N_MAX];         // cola del solapamiento-suma
    float re[N_MAX], im[N_MAX];     // trabajo de la FFT
} espectral_t;

int espectral_iniciar(espectral_t *e, int n);
// Recibe n/2 muestras nuevas y entrega n/2 muestras ya reconstruidas.
// Debe conservar la simetria conjugada para que la salida sea real.
int espectral_procesar_salto(espectral_t *e, const float *nuevas, float *salida,
                             const diagnostico_t *d, float fs);

#ifdef __cplusplus
}
#endif

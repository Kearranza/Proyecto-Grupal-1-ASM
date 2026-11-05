#pragma once
#include "config.h"
#include "diagnostico.h"
#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        int n;                      // tamano de bloque actual
        float umbral;               // intensidad en [0,1]; 0 conserva, 1 suprime (por defecto)
        float ancho_rechazo_hz;     // ancho total en Hz; 0 usa el diagnostico
        float entrada[N_MAX];       // ultimas n muestras
        float acumulado[N_MAX];     // cola del solapamiento-suma
        float re[N_MAX], im[N_MAX]; // trabajo de la FFT
    } espectral_t;

    // Reinicia el historial y los parametros. Usar al cambiar N o perder muestras.
    int espectral_iniciar(espectral_t *e, int n);
    // Recibe n/2 muestras nuevas y entrega n/2 muestras ya reconstruidas.
    // Retraso de N/2 muestras respecto a la secuencia de entrada.inicio con ceros.
    // Al terminar, un salto de ceros entrega las ultimas muestras. un segundo vacia
    // la cola del filtro.
    int espectral_procesar_salto(espectral_t *e, const float *nuevas, float *salida,
                                 const diagnostico_t *d, float fs);

#ifdef __cplusplus
}
#endif

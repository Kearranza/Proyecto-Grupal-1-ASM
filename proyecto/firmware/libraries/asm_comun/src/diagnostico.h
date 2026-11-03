// Nodo 1 (Luis): diagnostico del ruido a partir del espectro de un bloque.
// Frontera acordada: el nodo 1 ESTIMA; el nodo 2 ajusta su filtro con esto y el
// nodo 3 decide que bins modificar. Nadie mas vuelve a clasificar el ruido.
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RUIDO_NINGUNO = 0,
    RUIDO_TONAL = 1,       // interferencia sinusoidal (incluye 60 Hz)
    RUIDO_BLANCO = 2,      // banda ancha
    RUIDO_BANDA = 3,       // banda limitada
    RUIDO_IMPULSIVO = 4
} tipo_ruido_t;

// Se manda tal cual en la trama TRAMA_TIPO_DIAGNOSTICO (16 bytes, little-endian)
typedef struct __attribute__((packed)) {
    uint8_t tipo;           // tipo_ruido_t
    uint8_t reservado;
    uint16_t n_bloque;      // N con el que se calculo
    float frecuencia_hz;    // interferencia principal (0 si no aplica)
    float ancho_hz;         // ancho de la banda de ruido (0 si no aplica)
    float nivel_db;         // nivel estimado del ruido respecto a la senal
} diagnostico_t;

// magnitud: |X[k]| de k = 0 .. n/2 de un bloque con ventana Hann
int diagnostico_estimar(const float *magnitud, int n, float fs, diagnostico_t *salida);

#ifdef __cplusplus
}
#endif

// Nodo 1 (Luis): procesamiento de cada salto, independiente de Arduino.
// El sketch nodo1_pico_w.ino solo pone el ADC, el DMA, la UART y los comandos;
// todo lo que se puede probar en la computadora esta aqui (pruebas_c/test_nodo1.c).
#pragma once
#include <stdint.h>
#include "config.h"
#include "diagnostico.h"
#ifdef __cplusplus
extern "C" {
#endif

#define NODO1_ARRANCANDO 1   // todavia no hay un bloque completo: no hay diagnostico

typedef struct {
    int n;                        // tamano de bloque (N), salto = n/2
    int muestras_validas;         // cuenta el arranque hasta llenar el primer bloque
    float ventana[N_MAX];         // Hann periodica, se calcula una vez por N
    float bloque[N_MAX];          // ultimas n muestras, escaladas a [-1, 1)
    float x[N_MAX];               // bloque sin DC (lo usa el diagnostico en el tiempo)
    float re[N_MAX], im[N_MAX];   // trabajo de la FFT (se usan n/2)
    float giro_re[N_MAX / 2], giro_im[N_MAX / 2];   // e^(-j 2 pi k / n), para separar la FFT real
    float magnitud[N_MAX / 2 + 1];
} nodo1_t;

int nodo1_iniciar(nodo1_t *p, int n);
// nuevas: las n/2 muestras del salto, en cuentas del ADC ya centradas en 0
// (lectura - 2^(ADC_BITS-1)). Devuelve ASM_OK con 'd' listo, NODO1_ARRANCANDO
// mientras no se llena el primer bloque, o ASM_ERROR.
int nodo1_procesar_salto(nodo1_t *p, const int16_t *nuevas, diagnostico_t *d);
// Solo el paso 1 (deslizar el bloque), sin FFT ni diagnostico. Para cuando el nodo va
// atrasado: el bloque queda al dia y el salto siguiente se puede analizar normal.
// Devuelve ASM_OK, NODO1_ARRANCANDO o ASM_ERROR.
int nodo1_deslizar(nodo1_t *p, const int16_t *nuevas);
// Fuente simulada (comando FUENTE SIM): los tonos de config "fuente_sim" en cuentas
// del ADC centradas, desde la muestra 'primera'. Sirve para probar sin senal analogica.
void nodo1_generar_sim(uint32_t primera, int cantidad, int16_t *salida);

#ifdef __cplusplus
}
#endif

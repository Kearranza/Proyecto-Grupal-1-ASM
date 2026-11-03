// NODO 1 - Adquisicion y analisis (Luis) - Raspberry Pi Pico W
//
// Core de Arduino: "Raspberry Pi Pico/RP2040" (Earle Philhower). Placa: Pico W.
// Libreria: proyecto/firmware/libraries/asm_comun (ver proyecto/README.md).
//
// Flujo (seccion 1.1 del enunciado):
//   ADC0 (GPIO26) a FS_HZ por DMA -> saltos de N/2 muestras
//   core 0: quitar DC, ventana Hann, fft(), magnitud, diagnostico_estimar()
//   core 1: arma tramas (trama_armar) y las manda por UART1 (GP8) al ESP32
//
// Cada salto genera una trama de MUESTRAS (int16) y, cada bloque, una de
// DIAGNOSTICO. Si el DMA llena un bufer antes de que se procese el anterior,
// se cuenta un desbordamiento y se informa en una trama de ESTADO (requisito 5.3).
//
// Comandos por USB (sin recompilar), una orden por linea:
//   N <256|512|1024|2048>    cambia el tamano de bloque
//   ESTADO                   imprime N, saltos procesados, desbordamientos y tiempo de FFT

#include <Arduino.h>
#include <asm_comun.h>
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "pico/util/queue.h"

// ------------------------------------------------------------- estado
static int n_bloque = N_POR_DEFECTO;
static volatile uint32_t desbordamientos = 0;
static uint32_t saltos_procesados = 0;
static uint32_t us_ultima_fft = 0;   // medir esto primero: debe caber en N/(2 FS_HZ)

// Bufer ping-pong que llena el DMA (muestras crudas de 12 bits)
static uint16_t crudo[2][SALTO_MAX];

// Bloque de analisis: ultimas n_bloque muestras ya centradas
static float bloque[N_MAX];
static float re[N_MAX], im[N_MAX], magnitud[N_MAX / 2 + 1];

// Lo que el core 0 le pasa al core 1 para enviar
typedef struct {
    uint8_t tipo;
    uint16_t largo;
    uint8_t carga[SALTO_MAX * 2];
} pedido_envio_t;
static queue_t cola_envio;

// ------------------------------------------------------------- adquisicion
static void iniciar_adc_dma() {
    // TODO (Luis):
    //  adc_init(); adc_gpio_init(PICO_PIN_ADC); adc_select_input(0);
    //  adc_fifo_setup(true, true, 1, false, false);
    //  adc_set_clkdiv(48000000.0f / FS_HZ - 1);   // 48 MHz / 3000 = 16 kHz exactos
    //  dos canales DMA encadenados (ping-pong) con DREQ_ADC, cada uno de n_bloque/2
    //  muestras, y una IRQ de fin de transferencia que marque cual bufer esta listo.
    //  Si el bufer listo no se consumio todavia: desbordamientos++.
}

// Devuelve el indice del bufer listo (0 o 1) o -1 si todavia no hay
static int bufer_listo() {
    return -1;  // TODO (Luis): leer la bandera que pone la IRQ del DMA
}

// ------------------------------------------------------------- analisis
static void procesar_salto(const uint16_t *nuevas) {
    const int salto = n_bloque / 2;

    // 1. Enviar las muestras nuevas tal cual llegaron (int16 centradas en 0)
    static pedido_envio_t p;   // static: 2 KB no caben comodos en la pila
    p.tipo = TRAMA_TIPO_MUESTRAS;
    p.largo = (uint16_t)(salto * 2);
    int16_t *muestras = (int16_t *)p.carga;
    for (int i = 0; i < salto; i++) muestras[i] = (int16_t)nuevas[i] - (1 << (ADC_BITS - 1));
    if (!queue_try_add(&cola_envio, &p)) desbordamientos++;

    // 2. Ventana deslizante: corre la mitad vieja y agrega la nueva
    // TODO (Luis): memmove de bloque, copiar las nuevas, quitar la media (DC)

    // 3. Ventana Hann + FFT + magnitud
    uint32_t t0 = micros();
    // TODO (Luis): re[i] = bloque[i] * hann(i); im[i] = 0; fft(re, im, n_bloque);
    //              magnitud[k] = hypotf(re[k], im[k]) para k = 0 .. n_bloque/2
    us_ultima_fft = micros() - t0;

    // 4. Diagnostico y su trama
    diagnostico_t d;
    if (diagnostico_estimar(magnitud, n_bloque, FS_HZ, &d) == ASM_OK) {
        static pedido_envio_t pd;
        pd.tipo = TRAMA_TIPO_DIAGNOSTICO;
        pd.largo = sizeof(d);
        memcpy(pd.carga, &d, sizeof(d));
        queue_try_add(&cola_envio, &pd);
    }
    saltos_procesados++;
}

// ------------------------------------------------------------- comandos
static void atender_usb() {
    // TODO (Luis): leer una linea de Serial; "N 512" -> validar con n_es_valido(),
    // reiniciar el DMA con el nuevo salto; "ESTADO" -> imprimir contadores y us_ultima_fft
}

// ------------------------------------------------------------- core 0
void setup() {
    Serial.begin(115200);
    queue_init(&cola_envio, sizeof(pedido_envio_t), 4);
    iniciar_adc_dma();
}

void loop() {
    int i = bufer_listo();
    if (i >= 0) procesar_salto(crudo[i]);
    atender_usb();
}

// ------------------------------------------------------------- core 1: envio
static uint8_t trama[TRAMA_MAX_BYTES];
static uint16_t seq = 0;

void setup1() {
    Serial2.setTX(PICO_PIN_UART_TX);
    Serial2.setRX(PICO_PIN_UART_RX);
    Serial2.begin(BAUDIOS);
}

void loop1() {
    static pedido_envio_t p;
    if (queue_try_remove(&cola_envio, &p)) {
        int largo = trama_armar(p.tipo, seq++, p.carga, p.largo, trama, sizeof trama);
        if (largo > 0) Serial2.write(trama, largo);
    }
}

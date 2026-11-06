// NODO 1 - Adquisicion y analisis (Luis) - Raspberry Pi Pico W
//
// Core de Arduino: "Raspberry Pi Pico/RP2040" (Earle Philhower). Placa: Pico W.
// Libreria: proyecto/firmware/libraries/asm_comun (ver proyecto/README.md).
//
// Que hace (seccion 1.1 del enunciado):
//   core 0: ADC0 (GPIO26) a FS_HZ con DMA en ping-pong -> saltos de N/2 muestras
//           -> nodo1_procesar_salto() (quitar DC, Hann, FFT, diagnostico)
//           -> encola DIAGNOSTICO y despues MUESTRAS
//   core 1: arma cada trama (trama_armar) y la manda por UART1 (GP8) al ESP32
//
// El orden importa: el ESP32 le pega a cada salto el ultimo DIAGNOSTICO recibido,
// y el nodo 2 solo filtra si diagnostico.n_bloque == 2 x muestras del salto.
//
// Si al tomar un salto el otro bufer ya esta lleno, el nodo va atrasado: ese salto
// se manda y desliza el bloque pero no se analiza (saltos_sin_analizar). Asi las
// muestras llegan siempre continuas y solo baja la tasa de diagnosticos.
//
// Requisito 5.3: si el DMA vuelve a llenar un bufer que todavia no se proceso, se
// cuenta un desbordamiento; si la cola hacia el core 1 esta llena, un envio
// descartado. Los dos se ven con ESTADO y viajan en la trama ESTADO.
//
// Comandos por USB (115200, una orden por linea), sin recompilar:
//   N <256|512|1024|2048>     tamano de bloque (el ESP32 lo sigue solo)
//   FUENTE SIM | FUENTE ADC   SIM genera el modo 3 adentro: prueba sin senal analogica
//   UTIL <f1> [f2 ...]        frecuencias de la senal util que el diagnostico excluye
//   ANCHO_UTIL <hz>           ancho excluido alrededor de cada una
//   ESTADO | AYUDA

#include <Arduino.h>
#include <asm_comun.h>
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/util/queue.h"

// ------------------------------------------------------------- adquisicion (IRQ)
static uint16_t crudo[2][SALTO_MAX];   // ping-pong que llena el DMA
static volatile bool lleno[2];
static volatile uint32_t desbordamientos;
static int canal[2] = {-1, -1};
static int salto = N_POR_DEFECTO / 2;
static int siguiente;                  // bufer que toca procesar (alterna 0, 1)

// Se llama cuando un canal termina su bufer. El otro canal ya arranco solo
// (chain_to), asi que aqui solo se rearma este para la vuelta siguiente.
static void al_terminar_dma() {
    for (int i = 0; i < 2; i++) {
        if (canal[i] < 0 || !dma_channel_get_irq1_status(canal[i])) continue;
        dma_channel_acknowledge_irq1(canal[i]);
        dma_channel_set_write_addr(canal[i], crudo[i], false);
        dma_channel_set_trans_count(canal[i], salto, false);
        if (lleno[i]) desbordamientos++;   // nadie alcanzo a leer la vuelta anterior
        lleno[i] = true;
    }
}

static void detener_adquisicion() {
    adc_run(false);
    // Deshabilitar la IRQ antes de abortar: un abort puede levantarla (errata RP2040-E13)
    for (int i = 0; i < 2; i++) dma_channel_set_irq1_enabled(canal[i], false);
    for (int i = 0; i < 2; i++) dma_channel_abort(canal[i]);
    for (int i = 0; i < 2; i++) dma_channel_acknowledge_irq1(canal[i]);
    adc_fifo_drain();
}

static void arrancar_dma() {
    for (int i = 0; i < 2; i++) {
        dma_channel_config c = dma_channel_get_default_config(canal[i]);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
        channel_config_set_read_increment(&c, false);    // siempre el FIFO del ADC
        channel_config_set_write_increment(&c, true);
        channel_config_set_dreq(&c, DREQ_ADC);           // una transferencia por conversion
        channel_config_set_chain_to(&c, canal[1 - i]);   // al terminar arranca el otro
        dma_channel_configure(canal[i], &c, crudo[i], &adc_hw->fifo, salto, false);
        dma_channel_set_irq1_enabled(canal[i], true);
    }
    dma_channel_start(canal[0]);
    adc_run(true);
}

// ------------------------------------------------------------- procesamiento (core 0)
static nodo1_t nodo;
static bool fuente_sim = false;
static uint32_t muestra_sim, proximo_sim_us;
static uint32_t saltos_procesados, saltos_sin_analizar, us_ultimo, us_max;
static diagnostico_t ultimo_diagnostico;
static int ultimo_codigo = NODO1_ARRANCANDO;

typedef struct {
    uint8_t tipo;
    uint16_t largo;
    uint8_t carga[SALTO_MAX * 2];
} pedido_envio_t;
static queue_t cola_envio;
static volatile uint32_t envios_descartados;

static void encolar(uint8_t tipo, const void *carga, uint16_t largo) {
    static pedido_envio_t p;   // static: 2 KB no caben comodos en la pila
    p.tipo = tipo;
    p.largo = largo;
    memcpy(p.carga, carga, largo);
    if (!queue_try_add(&cola_envio, &p)) envios_descartados++;
}

// ESTADO: 3 x uint32 little-endian, igual que pc/registrar.py
// (desbordamientos, saltos_procesados, latencia_ms). latencia_ms es lo que espera
// una muestra en el nodo 1: llenar el salto (N/2 / fs) mas el computo del salto.
static void enviar_estado() {
    uint32_t carga[3] = {desbordamientos, saltos_procesados,
                         (uint32_t)(1000UL * salto / FS_HZ + (us_ultimo + 999) / 1000)};
    encolar(TRAMA_TIPO_ESTADO, carga, sizeof carga);
}

static void procesar_salto(const int16_t *nuevas, bool analizar) {
    if (!analizar) {
        nodo1_deslizar(&nodo, nuevas);
        encolar(TRAMA_TIPO_MUESTRAS, nuevas, (uint16_t)(salto * sizeof(int16_t)));
        saltos_sin_analizar++;
        saltos_procesados++;
        return;
    }
    uint32_t inicio = micros();
    diagnostico_t d;
    int r = nodo1_procesar_salto(&nodo, nuevas, &d);
    us_ultimo = micros() - inicio;
    if (us_ultimo > us_max) us_max = us_ultimo;
    ultimo_codigo = r;
    if (r == ASM_OK) {
        ultimo_diagnostico = d;
        encolar(TRAMA_TIPO_DIAGNOSTICO, &d, sizeof d);   // primero el diagnostico
    }
    encolar(TRAMA_TIPO_MUESTRAS, nuevas, (uint16_t)(salto * sizeof(int16_t)));
    saltos_procesados++;
}

static void reiniciar(int n) {
    detener_adquisicion();
    salto = n / 2;
    nodo1_iniciar(&nodo, n);
    lleno[0] = lleno[1] = false;
    siguiente = 0;
    us_max = 0;
    saltos_sin_analizar = 0;
    ultimo_codigo = NODO1_ARRANCANDO;
    if (fuente_sim) {
        muestra_sim = 0;
        proximo_sim_us = micros();
    } else {
        arrancar_dma();
    }
    enviar_estado();   // queda registrado en que bloque cambio N (docs/protocolo.md)
}

// ------------------------------------------------------------- comandos USB
static void imprimir_estado() {
    const uint32_t presupuesto = 1000000UL * salto / FS_HZ;
    static const char *tipos[] = {"ninguno", "tonal", "blanco", "banda", "impulsivo"};
    Serial.printf("N=%d fuente=%s saltos=%lu sin_analizar=%lu desbordamientos=%lu envios_descartados=%lu\n",
                  nodo.n, fuente_sim ? "SIM" : "ADC", (unsigned long)saltos_procesados,
                  (unsigned long)saltos_sin_analizar, (unsigned long)desbordamientos,
                  (unsigned long)envios_descartados);
    Serial.printf("computo_us ultimo=%lu max=%lu presupuesto=%lu carga=%.1f%%\n",
                  (unsigned long)us_ultimo, (unsigned long)us_max, (unsigned long)presupuesto,
                  100.0f * us_ultimo / presupuesto);
    if (ultimo_codigo == NODO1_ARRANCANDO) {
        Serial.println("diagnostico=arrancando");
    } else {
        const diagnostico_t &d = ultimo_diagnostico;
        Serial.printf("diagnostico=%s frecuencia_hz=%.1f ancho_hz=%.1f nivel_db=%.1f\n",
                      d.tipo <= RUIDO_IMPULSIVO ? tipos[d.tipo] : "?", d.frecuencia_hz,
                      d.ancho_hz, d.nivel_db);
    }
}

// Bandas de la senal util que el diagnostico excluye (por defecto, config/sistema.json)
static float centros_util[DIAG_MAX_UTILES] = DIAG_UTILES_HZ;
static int n_utiles = DIAG_N_UTILES;
static float ancho_util = DIAG_ANCHO_UTIL_HZ;

static void aplicar_utiles() {
    if (diagnostico_configurar_utiles(centros_util, n_utiles, ancho_util) == ASM_OK)
        Serial.printf("OK %d frecuencias utiles, +-%.0f Hz\n", n_utiles, ancho_util);
    else
        Serial.println("ERROR bandas utiles");
}

static void ejecutar_comando(char *linea) {
    char *orden = strtok(linea, " ");
    if (!orden) return;
    if (!strcmp(orden, "ESTADO")) {
        imprimir_estado();
    } else if (!strcmp(orden, "N")) {
        char *v = strtok(nullptr, " ");
        int n = v ? atoi(v) : 0;
        if (n < 256 || !n_es_valido(n)) { Serial.println("ERROR N: 256, 512, 1024 o 2048"); return; }
        reiniciar(n);
        Serial.printf("OK N=%d\n", n);
    } else if (!strcmp(orden, "FUENTE")) {
        char *v = strtok(nullptr, " ");
        if (!v || (strcmp(v, "SIM") && strcmp(v, "ADC"))) { Serial.println("ERROR FUENTE SIM|ADC"); return; }
        fuente_sim = !strcmp(v, "SIM");
        reiniciar(nodo.n);
        Serial.printf("OK FUENTE %s\n", v);
    } else if (!strcmp(orden, "UTIL")) {
        int cuantos = 0;
        for (char *v = strtok(nullptr, " "); v && cuantos < DIAG_MAX_UTILES; v = strtok(nullptr, " "))
            centros_util[cuantos++] = (float)atof(v);
        n_utiles = cuantos;
        aplicar_utiles();
    } else if (!strcmp(orden, "ANCHO_UTIL")) {
        char *v = strtok(nullptr, " ");
        if (!v || atof(v) < 0) { Serial.println("ERROR ANCHO_UTIL <hz>"); return; }
        ancho_util = (float)atof(v);
        aplicar_utiles();
    } else if (!strcmp(orden, "AYUDA")) {
        Serial.println("N 256|512|1024|2048 | FUENTE SIM|ADC | UTIL f1 [f2 ...] | ANCHO_UTIL hz | ESTADO");
    } else {
        Serial.println("ERROR comando; use AYUDA");
    }
}

static void atender_usb() {
    static char linea[96];
    static unsigned usados;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            linea[usados] = 0;
            if (usados) ejecutar_comando(linea);
            usados = 0;
        } else if (usados + 1 < sizeof linea) {
            linea[usados++] = c;
        }
    }
}

// ------------------------------------------------------------- core 0
void setup() {
    Serial.begin(115200);
    queue_init(&cola_envio, sizeof(pedido_envio_t), 8);

    adc_init();
    adc_gpio_init(PICO_PIN_ADC);
    adc_select_input(PICO_PIN_ADC - 26);           // GPIO26 = ADC0
    adc_fifo_setup(true, true, 1, false, false);   // FIFO con DREQ, 12 bits sin recortar
    adc_set_clkdiv(48000000.0f / FS_HZ - 1.0f);    // 48 MHz / 3000 = 16 kHz exactos

    for (int i = 0; i < 2; i++) canal[i] = dma_claim_unused_channel(true);
    // Compartida: el core de Arduino puede usar DMA_IRQ_1 para otras cosas
    irq_add_shared_handler(DMA_IRQ_1, al_terminar_dma, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(DMA_IRQ_1, true);

    reiniciar(N_POR_DEFECTO);
}

void loop() {
    static int16_t nuevas[SALTO_MAX];
    static uint32_t ultimo_estado_ms;

    if (fuente_sim) {
        // Un salto cada N/2 / fs segundos, con el reloj acumulado para no derivar
        if ((int32_t)(micros() - proximo_sim_us) >= 0) {
            proximo_sim_us += 1000000UL * salto / FS_HZ;
            nodo1_generar_sim(muestra_sim, salto, nuevas);
            muestra_sim += salto;
            procesar_salto(nuevas, true);
        }
    } else if (lleno[siguiente]) {
        // Se copia y se libera enseguida: el DMA puede volver a este bufer mientras
        // se procesa la copia, y se gana un salto entero de margen
        const int16_t centro = 1 << (ADC_BITS - 1);
        for (int i = 0; i < salto; i++) nuevas[i] = (int16_t)crudo[siguiente][i] - centro;
        lleno[siguiente] = false;
        siguiente = 1 - siguiente;
        // El otro bufer ya lleno: vamos un salto atrasados, este no se analiza
        procesar_salto(nuevas, !lleno[siguiente]);
    }

    // ESTADO como maximo una vez por segundo (docs/protocolo.md)
    if (millis() - ultimo_estado_ms >= 1000) {
        ultimo_estado_ms = millis();
        enviar_estado();
    }
    atender_usb();
}

// ------------------------------------------------------------- core 1: envio
static uint8_t trama[TRAMA_MAX_BYTES];
static uint16_t seq;

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

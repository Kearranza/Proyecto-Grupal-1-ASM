// NODOS 2 y 3 - Filtrado temporal y espectral - ESP32 clasico (con DAC)
//
// Base del sketch: Luis (recepcion, tareas, salidas).
// Procesamiento: Santiago (iir.c, nodo 2) y Gretchell (espectral.c y metricas.c, nodo 3).
// Core de Arduino: "esp32" de Espressif, version 3.x. Placa: ESP32 Dev Module.
//
// El enunciado permite juntar los nodos 2 y 3 en un micro multinucleo si hay dos
// rutas independientes y dos salidas comparables:
//   tarea "recepcion"  (core 0): UART2 (GPIO16) -> trama_leer_byte -> reparte saltos
//   tarea "temporal"   (core 0): iir_procesar          -> cola de salida DAC1 (GPIO25)
//   tarea "espectral"  (core 1): espectral_procesar_salto + metricas -> DAC2 (GPIO26)
//   interrupcion a FS_HZ: saca una muestra de cada cola por su DAC
//
// Comandos por USB (sin recompilar), una orden por linea:
//   N <n>  |  UMBRAL <x>  |  ANCHO <hz>  |  ESTADO

#include <Arduino.h>
#include <asm_comun.h>

// ------------------------------------------------------------- estado compartido
static diagnostico_t diagnostico = {RUIDO_NINGUNO, 0, N_POR_DEFECTO, 0, 0, 0};
static portMUX_TYPE candado_diagnostico = portMUX_INITIALIZER_UNLOCKED;
static lector_trama_t lector;
static trama_t trama;

static QueueHandle_t cola_temporal, cola_espectral;   // saltos de entrada (float)
// TODO (Luis): dos bufers circulares de salida (uno por DAC) que llenan las tareas
// y vacia la interrupcion. Si se vacian: contar "subdesbordamiento" (requisito 5.3).

typedef struct {
    uint16_t n;          // muestras validas
    float x[SALTO_MAX];
} salto_t;

// ------------------------------------------------------------- salidas
// Interfaz acordada: muestra en [-1, 1) -> nivel del DAC de 8 bits centrado
static inline void salida_dac(int canal, float muestra) {
    int nivel = (int)lroundf(muestra * 128.0f) + 128;
    if (nivel < 0) nivel = 0;
    if (nivel > 255) nivel = 255;
    dacWrite(canal == 0 ? ESP_PIN_DAC_TEMPORAL : ESP_PIN_DAC_ESPECTRAL, nivel);
}

static hw_timer_t *temporizador = nullptr;

void IRAM_ATTR al_muestrear() {
    // TODO (Luis): sacar una muestra de cada bufer de salida y llamar salida_dac().
    // Verificar que dacWrite se pueda llamar desde la interrupcion en el core 3.x;
    // si no, usar el driver dac_continuous (DMA) de ESP-IDF para las dos salidas.
}

// ------------------------------------------------------------- tareas
static void tarea_recepcion(void *) {
    static salto_t s;
    for (;;) {
        while (Serial2.available()) {
            if (trama_leer_byte(&lector, (uint8_t)Serial2.read(), &trama) != 1) continue;
            if (trama.tipo == TRAMA_TIPO_MUESTRAS) {
                s.n = trama.largo / 2;
                const int16_t *m = (const int16_t *)trama.carga;
                for (int i = 0; i < s.n; i++) s.x[i] = m[i] / (float)(1 << (ADC_BITS - 1));
                // Si una cola esta llena, ese nodo no alcanza a procesar: se cuenta
                if (xQueueSend(cola_temporal, &s, 0) != pdTRUE) { /* TODO (Luis): contador */ }
                if (xQueueSend(cola_espectral, &s, 0) != pdTRUE) { /* TODO (Luis): contador */ }
            } else if (trama.tipo == TRAMA_TIPO_DIAGNOSTICO && trama.largo == sizeof(diagnostico_t)) {
                portENTER_CRITICAL(&candado_diagnostico);
                memcpy(&diagnostico, trama.carga, sizeof(diagnostico_t));
                portEXIT_CRITICAL(&candado_diagnostico);
            }
        }
        vTaskDelay(1);
    }
}

static diagnostico_t leer_diagnostico() {
    portENTER_CRITICAL(&candado_diagnostico);
    diagnostico_t d = diagnostico;
    portEXIT_CRITICAL(&candado_diagnostico);
    return d;
}

static void tarea_temporal(void *) {
    // NODO 2 (Santiago)
    static iir_t filtro;
    static salto_t s;
    static float y[SALTO_MAX];
    uint8_t tipo_actual = 0xFF;
    float frecuencia_actual = -1.0f;
    for (;;) {
        if (xQueueReceive(cola_temporal, &s, portMAX_DELAY) != pdTRUE) continue;
        diagnostico_t d = leer_diagnostico();
        if (d.tipo != tipo_actual || d.frecuencia_hz != frecuencia_actual) {
            // TODO (Santiago): rediseñar solo si el cambio es significativo
            if (iir_disenar(&filtro, &d, FS_HZ) == ASM_OK) iir_reiniciar_estado(&filtro);
            tipo_actual = d.tipo;
            frecuencia_actual = d.frecuencia_hz;
        }
        iir_procesar(&filtro, s.x, y, s.n);
        // TODO (Luis): poner y[] en el bufer de salida del DAC1
    }
}

static void tarea_espectral(void *) {
    // NODO 3 (Gretchell)
    static espectral_t e;
    static salto_t s;
    static float y[SALTO_MAX];
    espectral_iniciar(&e, N_POR_DEFECTO);
    for (;;) {
        if (xQueueReceive(cola_espectral, &s, portMAX_DELAY) != pdTRUE) continue;
        diagnostico_t d = leer_diagnostico();
        uint32_t t0 = micros();
        espectral_procesar_salto(&e, s.x, y, &d, FS_HZ);
        uint32_t latencia_us = micros() - t0;
        (void)latencia_us;
        // TODO (Gretchell): metricas_acumular() con la referencia limpia del modo,
        //   mostrar metodo activo, energia, MSE y SNR antes/despues (pantalla o interfaz)
        // TODO (Luis): poner y[] en el bufer de salida del DAC2
    }
}

// ------------------------------------------------------------- comandos
static void atender_usb() {
    // TODO (Luis): N, UMBRAL, ANCHO, ESTADO. ESTADO imprime lector.tramas_ok,
    // lector.errores_crc, lector.bloques_perdidos y los contadores de colas.
}

void setup() {
    Serial.begin(115200);
    Serial2.setRxBufferSize(16384);
    Serial2.begin(BAUDIOS, SERIAL_8N1, ESP_PIN_UART_RX, ESP_PIN_UART_TX);
    trama_lector_iniciar(&lector);

    cola_temporal = xQueueCreate(4, sizeof(salto_t));
    cola_espectral = xQueueCreate(4, sizeof(salto_t));
    xTaskCreatePinnedToCore(tarea_recepcion, "recepcion", 8192, nullptr, 3, nullptr, 0);
    xTaskCreatePinnedToCore(tarea_temporal, "temporal", 8192, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(tarea_espectral, "espectral", 16384, nullptr, 2, nullptr, 1);

    // 80 MHz / 5 = 16 MHz; alarma cada 16 MHz / FS_HZ = 1000 ticks -> 16 kHz exactos
    temporizador = timerBegin(16000000);
    timerAttachInterrupt(temporizador, &al_muestrear);
    timerAlarm(temporizador, 16000000 / FS_HZ, true, 0);
}

void loop() {
    atender_usb();
    delay(10);
}

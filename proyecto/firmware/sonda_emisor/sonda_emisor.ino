// Sonda y emisora en una sola Pico (Kevin).
//
// Con tres placas no alcanza para tener emisora y sonda por separado: la Pico W es
// el nodo 1 y el ESP32 son los nodos 2 y 3, asi que la Pico que queda tiene que
// hacer las dos cosas. Se puede porque son periféricos distintos y no se estorban:
//
//   PWM en GP15  -> RC de 2 etapas -> Pico W GPIO26    emite la senal de prueba
//   UART1 en GP9 <- Pico W GP8                         escucha el enlace
//   USB          -> pc/registrar.py                    entrega las tramas
//
// La interrupcion de muestreo solo hace una lectura de tabla y una escritura de PWM,
// y como interrumpe al lazo principal, el reenvio de bytes no le altera el tiempo.
//
// Comandos por USB (115200, una orden por linea):
//   TOCAR | PARAR | LAZO ON|OFF | ESTADO
//
// CUIDADO con el texto: el lector de trama.py resincroniza con AA 55, asi que texto
// ENTRE tramas no estorba (40 de 40 recuperadas al medirlo), pero texto que parte
// una trama las pierde todas (0 de 40). Por eso este sketch lleva la cuenta de en
// que punto de la trama va y solo deja salir texto cuando no hay una a medias.

#include <Arduino.h>
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include <asm_comun.h>
#include "tabla_senal.h"

const uint8_t PIN_PWM     = 15;                  // igual que la emisora de tonos
const uint8_t PIN_ESCUCHA = PICO_PIN_UART_RX;    // GP9, pin fisico 12
const uint8_t PIN_TX_LIBRE = 4;                  // UART1 TX a un pin sin conectar
const uint8_t REPOSO = 128;                      // portadora al 50 %: silencio
const size_t  TROZO = 256;

static uint slice_portadora, slice_reloj = 0;
static double fs_real;
static volatile uint32_t indice, vueltas;
static volatile bool tocando, en_lazo = true;

static uint8_t buzon[TROZO];
static uint32_t bytes_reenviados, ultimo_parpadeo;

// ---- seguimiento de la trama: solo para saber si hay uno a medias
static uint8_t etapa, cabecera[7];
static uint16_t faltan;

static void seguir(uint8_t b) {
    // El cuerpo va primero: si no, etapa 8 caeria en cabecera[8] y se sale del arreglo
    if (etapa >= 8) { if (--faltan == 0) etapa = 0; return; }
    if (etapa == 0) { etapa = (b == TRAMA_SYNC0); return; }
    if (etapa == 1) { etapa = (b == TRAMA_SYNC1) ? 2 : (b == TRAMA_SYNC0); return; }
    cabecera[etapa++] = b;
    if (etapa == 7) {                      // tipo + seq(2) + largo(2) ya leidos
        uint16_t largo = (uint16_t)(cabecera[5] | (cabecera[6] << 8));
        faltan = (largo > TRAMA_CARGA_MAX) ? 0 : (uint16_t)(largo + 2);  // + CRC
        etapa = faltan ? 8 : 0;
    }
}

static bool entre_tramas() { return etapa == 0; }

static void al_muestrear() {
    pwm_clear_irq(slice_reloj);
    if (!tocando) { pwm_set_chan_level(slice_portadora, PWM_CHAN_B, REPOSO); return; }
    pwm_set_chan_level(slice_portadora, PWM_CHAN_B, TABLA_SENAL[indice]);
    if (++indice >= TABLA_N) {
        indice = 0; vueltas++;
        if (!en_lazo) tocando = false;
    }
}

static void configurar_pwm() {
    const uint32_t clk = clock_get_hz(clk_sys);
    gpio_set_function(PIN_PWM, GPIO_FUNC_PWM);
    slice_portadora = pwm_gpio_to_slice_num(PIN_PWM);
    pwm_config c = pwm_get_default_config();
    pwm_config_set_clkdiv(&c, 1.0f);
    pwm_config_set_wrap(&c, 255);
    pwm_init(slice_portadora, &c, true);
    pwm_set_chan_level(slice_portadora, PWM_CHAN_B, REPOSO);

    if (slice_reloj == slice_portadora) slice_reloj = 1;
    const uint32_t cuentas = (uint32_t)(((uint64_t)clk * 2) / (5ULL * TABLA_FS_HZ));
    fs_real = (double)clk / (2.5 * (double)cuentas);
    pwm_config r = pwm_get_default_config();
    pwm_config_set_clkdiv(&r, 2.5f);
    pwm_config_set_wrap(&r, cuentas - 1);
    pwm_init(slice_reloj, &r, false);
    pwm_clear_irq(slice_reloj);
    pwm_set_irq_enabled(slice_reloj, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, al_muestrear);
    irq_set_enabled(PWM_IRQ_WRAP, true);
    pwm_set_enabled(slice_reloj, true);
}

// Las respuestas esperan a que no haya una trama a medias
static char pendiente[96];
static void responder(const char *texto) { strncpy(pendiente, texto, sizeof pendiente - 1); }

static void atender(char *linea) {
    char *orden = strtok(linea, " \t");
    if (!orden) return;
    if (!strcmp(orden, "TOCAR"))      { indice = 0; vueltas = 0; tocando = true;  responder("OK TOCAR"); }
    else if (!strcmp(orden, "PARAR")) { tocando = false;                          responder("OK PARAR"); }
    else if (!strcmp(orden, "LAZO")) {
        const char *v = strtok(NULL, " \t");
        if (!v || (strcmp(v, "ON") && strcmp(v, "OFF"))) { responder("ERROR LAZO ON|OFF"); return; }
        en_lazo = !strcmp(v, "ON");
        responder(en_lazo ? "OK LAZO ON" : "OK LAZO OFF");
    } else if (!strcmp(orden, "ESTADO")) {
        static char buf[96];
        snprintf(buf, sizeof buf, "tocando=%d lazo=%d vueltas=%lu fs=%.1f bytes=%lu",
                 tocando ? 1 : 0, en_lazo ? 1 : 0, (unsigned long)vueltas,
                 fs_real, (unsigned long)bytes_reenviados);
        responder(buf);
    } else responder("ERROR use TOCAR|PARAR|LAZO ON|OFF|ESTADO");
}

void setup() {
    Serial.begin(115200);
    Serial2.setTX(PIN_TX_LIBRE);          // para no pelear con el GP8 del nodo 1
    Serial2.setRX(PIN_ESCUCHA);
    Serial2.setFIFOSize(4096);
    Serial2.begin(BAUDIOS);
    pinMode(LED_BUILTIN, OUTPUT);
    configurar_pwm();
}

void loop() {
    int hay = Serial2.available();
    if (hay > 0) {
        size_t n = Serial2.readBytes(buzon, min((size_t)hay, TROZO));
        for (size_t i = 0; i < n; i++) seguir(buzon[i]);
        Serial.write(buzon, n);
        bytes_reenviados += n;
    }

    static char linea[64]; static size_t largo;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') { if (largo) { linea[largo] = '\0'; atender(linea); largo = 0; } }
        else if (largo < sizeof linea - 1) linea[largo++] = c;
    }
    if (pendiente[0] && entre_tramas()) { Serial.println(pendiente); pendiente[0] = '\0'; }

    uint32_t ahora = millis();
    if (ahora - ultimo_parpadeo >= 500) {
        ultimo_parpadeo = ahora;
        digitalWrite(LED_BUILTIN, bytes_reenviados > 0 ? HIGH : !digitalRead(LED_BUILTIN));
        bytes_reenviados = 0;
    }
}

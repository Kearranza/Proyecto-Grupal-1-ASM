// Emisora de senal arbitraria (Kevin). Reproduce un WAV guardado como tabla.
//
// Para P3 hace falta una senal que no sea multitono. En vez de meterla por la
// tarjeta de sonido, que pide capacitor de acople, divisor y cable de 3,5 mm y
// puede danar el ADC si se conecta mal, sale por el mismo camino que P1 y P2:
// PWM en GP15 -> RC de 2 etapas -> GPIO26 de la Pico W. La senal queda identica
// en cada repeticion, que es lo que pide una campana con 3 repeticiones.
//
// La tabla la genera pc/tabla_senal.py desde senales/p3.wav, que es ademas la
// referencia con la que campana.py calcula las metricas.
//
// Comandos por USB (115200, una orden por linea):
//   TOCAR | PARAR | LAZO ON|OFF | ESTADO
//
// El reloj de muestreo y la portadora replican el esquema del emisor de tonos
// del taller, para que las dos emisoras se comporten igual en el mismo RC.

#include <Arduino.h>
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "tabla_senal.h"

const uint8_t PIN_PWM = 15;          // mismo pin que la emisora de tonos
const uint8_t REPOSO  = 128;         // portadora al 50 %: silencio

static uint slice_portadora, slice_reloj = 0;
static double fs_real;
static volatile uint32_t indice;
static volatile bool tocando, en_lazo = true;
static volatile uint32_t vueltas;

static void al_muestrear() {
    pwm_clear_irq(slice_reloj);
    if (!tocando) {
        pwm_set_chan_level(slice_portadora, PWM_CHAN_B, REPOSO);
        return;
    }
    pwm_set_chan_level(slice_portadora, PWM_CHAN_B, TABLA_SENAL[indice]);
    if (++indice >= TABLA_N) {
        indice = 0;
        vueltas++;
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

static void estado() {
    Serial.printf("tocando=%d lazo=%d indice=%lu/%d vueltas=%lu\n",
                  tocando ? 1 : 0, en_lazo ? 1 : 0,
                  (unsigned long)indice, TABLA_N, (unsigned long)vueltas);
    Serial.printf("fs_real=%.3f Hz portadora=%.0f Hz duracion=%.3f s\n",
                  fs_real, (double)clock_get_hz(clk_sys)/256.0,
                  (double)TABLA_N/fs_real);
}

static void atender(char *linea) {
    char *orden = strtok(linea, " \t");
    if (!orden) return;
    if (!strcmp(orden, "TOCAR")) {
        indice = 0; vueltas = 0; tocando = true;
        Serial.println("OK TOCAR");
    } else if (!strcmp(orden, "PARAR")) {
        tocando = false;
        Serial.println("OK PARAR");
    } else if (!strcmp(orden, "LAZO")) {
        const char *v = strtok(NULL, " \t");
        if (!v || (strcmp(v, "ON") && strcmp(v, "OFF"))) {
            Serial.println("ERROR LAZO ON|OFF"); return;
        }
        en_lazo = !strcmp(v, "ON");
        Serial.printf("OK LAZO %s\n", v);
    } else if (!strcmp(orden, "ESTADO")) {
        estado();
    } else {
        Serial.println("ERROR comando; use TOCAR|PARAR|LAZO ON|OFF|ESTADO");
    }
}

void setup() {
    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 2000) delay(10);
    configurar_pwm();
    Serial.printf("emisora de tabla: %d muestras a %d Hz (%.3f s)\n",
                  TABLA_N, TABLA_FS_HZ, (double)TABLA_N/TABLA_FS_HZ);
}

void loop() {
    static char linea[64];
    static size_t n;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (n) { linea[n] = '\0'; atender(linea); n = 0; }
        } else if (n < sizeof linea - 1) {
            linea[n++] = c;
        }
    }
}

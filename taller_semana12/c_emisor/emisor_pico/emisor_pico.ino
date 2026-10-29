// BLOQUE C - Emisor en la Raspberry Pi Pico (rama: taller-emisor)
//
// Punto 2 del taller: generar en el micro las frecuencias establecidas en
// taller_semana12/parametros.json, a fs = 16 kHz, por PWM de 8 bits + filtro RC.
//
// Entorno: Arduino IDE con el core "Raspberry Pi Pico/RP2040" de Earle Philhower
// (trae el SDK de la Pico, por eso se pueden incluir hardware/pwm.h, etc.).
// MicroPython no alcanza para una interrupcion cada 62,5 us con 4 senos.
//
// Dos rebanadas (slices) de PWM:
//   - SALIDA (GPIO15): portadora de 8 bits, tope 255 -> clk_sys/256 (~488 kHz
//     a 125 MHz). El ciclo de trabajo es la muestra; el RC deja pasar los
//     kHz de la senal y quita la portadora.
//   - RELOJ (rebanada 0, sin pin): solo sirve para generar la interrupcion de
//     muestreo. Con divisor 2,5 (exacto en el divisor fraccionario 8.4) y
//     tope = clk/(2,5*fs) - 1 da 16 kHz exactos a 125 o 133 MHz.
//
// Metodo: DDS (sintesis digital directa). Cada componente tiene un acumulador de
// fase de 32 bits; en cada muestra se le suma
//     incremento = round(f * 2^32 / fs)
// y los TABLA_BITS bits altos de la fase indexan TABLA_SENO. Cambiar una
// frecuencia es cambiar un numero, sin recompilar.
//
// Protocolo serial (USB, una orden por linea). Lo usa control_emisor.py:
//   MODO <n>                 carga el modo n (1, 2, 3)
//   TONO <i> <f_hz> <amp>    fija la componente i (amp en [0, 1])
//   ESTADO                   imprime fs real y las componentes activas
// Respuestas: "OK ..." o "ERR ...".

#include <Arduino.h>
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "tabla_seno.h"

// ---------------------------------------------------------------- configuracion
static const uint32_t FS_HZ = 16000;        // = fs_hz de configuraciones.json
static const uint PIN_SALIDA = 15;          // PWM7 B
static const uint16_t PWM_TOPE = 255;       // 8 bits = emisor.bits_salida
static const uint REBANADA_RELOJ = 0;       // no se conecta a ningun pin
static const float DIVISOR_RELOJ = 2.5f;
static const float NIVEL_PICO = 0.8f;       // = emisor.nivel_pico
static const int MAX_COMPONENTES = 4;

struct Componente {
  uint32_t fase;
  uint32_t incremento;
  int16_t amplitud_q15;  // 0 = apagada
};

static volatile Componente componentes[MAX_COMPONENTES];
static volatile int32_t escala_q15 = 0;     // NIVEL_PICO / suma de amplitudes
static float fs_real = 0.0f;

// ---------------------------------------------------------------- utilidades
static uint32_t incremento_de(float f_hz) {
  // TODO (C): round(f * 2^32 / fs_real). Calcular en double para no desbordar.
  return 0;
}

static void fijar_componente(int i, float f_hz, float amplitud) {
  // TODO (C): validar i y f < FS_HZ/2, guardar incremento y amplitud en Q15,
  // y recalcular escala_q15 para que la suma nunca pase de NIVEL_PICO.
}

static void cargar_modo(int modo) {
  // TODO (C): mismos valores que parametros.json -> emisor.modos
  //   1: 440 Hz
  //   2: 440, 880, 1320 Hz con 1.0, 0.55, 0.3
  //   3: modo 2 + 3500 Hz con 0.9373
}

// ---------------------------------------------------------------- muestreo
static void al_muestrear() {
  pwm_clear_irq(REBANADA_RELOJ);
  // TODO (C): por cada componente activa
  //   acumulado += amplitud_q15 * TABLA_SENO[fase >> (32 - TABLA_BITS)]
  //   fase += incremento
  // aplicar escala_q15, pasar de [-1, 1) a 0..PWM_TOPE centrado en la mitad y
  //   pwm_set_gpio_level(PIN_SALIDA, nivel);
}

static void configurar_pwm() {
  // Portadora en el pin de salida, arranca en 50 % (0 V de senal tras el RC)
  gpio_set_function(PIN_SALIDA, GPIO_FUNC_PWM);
  uint rebanada_salida = pwm_gpio_to_slice_num(PIN_SALIDA);
  pwm_config salida = pwm_get_default_config();
  pwm_config_set_wrap(&salida, PWM_TOPE);
  pwm_config_set_clkdiv(&salida, 1.0f);
  pwm_init(rebanada_salida, &salida, true);
  pwm_set_gpio_level(PIN_SALIDA, (PWM_TOPE + 1) / 2);

  // Reloj de muestreo: la interrupcion de fin de ciclo de esta rebanada es fs
  uint32_t clk = clock_get_hz(clk_sys);
  uint32_t tope = (uint32_t)lroundf(clk / (DIVISOR_RELOJ * FS_HZ)) - 1;
  fs_real = clk / (DIVISOR_RELOJ * (tope + 1));
  pwm_config reloj = pwm_get_default_config();
  pwm_config_set_clkdiv(&reloj, DIVISOR_RELOJ);
  pwm_config_set_wrap(&reloj, tope);
  pwm_init(REBANADA_RELOJ, &reloj, false);

  pwm_clear_irq(REBANADA_RELOJ);
  pwm_set_irq_enabled(REBANADA_RELOJ, true);
  irq_set_exclusive_handler(PWM_IRQ_WRAP, al_muestrear);
  irq_set_enabled(PWM_IRQ_WRAP, true);
  pwm_set_enabled(REBANADA_RELOJ, true);
}

// ---------------------------------------------------------------- serial
static void atender_serial() {
  // TODO (C): leer una linea, separar la orden y sus argumentos, llamar a
  // cargar_modo() o fijar_componente() y responder OK / ERR.
}

void setup() {
  Serial.begin(115200);
  configurar_pwm();
  cargar_modo(1);
  Serial.print("OK emisor listo, fs real = ");
  Serial.println(fs_real, 3);
}

void loop() {
  atender_serial();
}

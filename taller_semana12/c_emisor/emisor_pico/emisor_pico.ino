/*
 * emisor_pico.ino  --  Taller semana 12, bloque C (emisor en la Pico)
 *
 * La Pico no tiene DAC. La senal se sintetiza por DDS (acumulador de fase +
 * tabla de seno) y sale por PWM de 8 bits en GPIO15; un filtro RC de dos
 * etapas elimina la portadora y deja la senal analogica.
 *
 *   Reloj de muestreo : slice PWM 0, divisor 2.5, IRQ de wrap -> fs = 16 kHz exactos
 *   Portadora PWM     : slice de GPIO15, tope 255 -> clk_sys/256 (488 kHz a 125 MHz)
 *   Lazo de captura   : GPIO26 (ADC0) lee la salida del RC dentro de la misma IRQ,
 *                       sincronizada con el reloj de muestreo.
 *
 * Comandos por serial (terminan en \n):
 *   MODO n    n = 1, 2 o 3        -> responde "OK n"
 *   CAPTURA                       -> envia 1024 lecturas del ADC (CAP_INICIO ... CAP_FIN)
 *   INFO                          -> modo, fs real y portadora real
 *
 * Los numeros de los modos deben coincidir con taller_semana12/parametros.json.
 * Herramientas > CPU Speed: 125 MHz (con 133 MHz tambien funciona; la fs sigue
 * siendo exacta, solo cambia la frecuencia de la portadora).
 */
#include <Arduino.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"

// ------------------------------------------------------------- parametros
const uint8_t  PIN_PWM    = 15;       // salida PWM (hacia el RC)
const uint8_t  PIN_ADC    = 26;       // ADC0, lee la salida del RC
const uint32_t FS_HZ      = 16000;    // = fs de configuraciones.json
const float    NIVEL_PICO = 0.8f;     // = emisor.nivel_pico de parametros.json
const uint16_t N_CAPTURA  = 1024;     // = N de configuraciones.json
const uint8_t  MAX_TONOS  = 4;

struct Modo {
  uint8_t nt;
  float   f[MAX_TONOS];   // Hz
  float   a[MAX_TONOS];   // amplitud relativa
};

const Modo MODOS[3] = {
  {1, {440.0f,    0.0f,    0.0f,    0.0f},   {1.0f, 0.0f,  0.0f, 0.0f}},
  {3, {440.0f,  880.0f, 1320.0f,    0.0f},   {1.0f, 0.55f, 0.3f, 0.0f}},
  {4, {440.0f,  880.0f, 1320.0f, 3500.0f},   {1.0f, 0.55f, 0.3f, 0.9373f}},
};

// ----------------------------------------------------------------- estado
static int16_t SENO[1024];                 // seno en Q15, 1024 puntos

volatile uint8_t  nt = 0;
volatile uint32_t fase[MAX_TONOS];
volatile uint32_t inc[MAX_TONOS];
volatile int32_t  amp[MAX_TONOS];          // amplitud en Q15
volatile uint8_t  modo_actual = 0;

static uint   slice_portadora;
static uint   slice_reloj = 0;
static double fs_real  = FS_HZ;
static double f_portadora = 0;

volatile bool     capturando = false;
volatile bool     captura_lista = false;
volatile uint16_t idx_cap = 0;
static uint16_t   cap[N_CAPTURA];

// ------------------------------------------------------ IRQ: una por muestra
void __not_in_flash_func(on_muestra)() {
  pwm_clear_irq(slice_reloj);

  if (capturando) {                        // lectura del ADC sincronizada con fs
    cap[idx_cap++] = adc_read();
    if (idx_cap >= N_CAPTURA) { capturando = false; captura_lista = true; }
  }

  int32_t acc = 0;                         // suma de tonos, Q15
  for (uint8_t i = 0; i < nt; i++) {
    fase[i] += inc[i];
    acc += (amp[i] * (int32_t)SENO[fase[i] >> 22]) >> 15;
  }
  int32_t duty = 128 + (acc >> 8);         // centro 1.65 V, escala 8 bits
  if (duty < 0)   duty = 0;
  if (duty > 255) duty = 255;
  pwm_set_chan_level(slice_portadora, PWM_CHAN_B, (uint16_t)duty);
}

// ------------------------------------------------------------ configuracion
void construir_tabla() {
  for (int i = 0; i < 1024; i++)
    SENO[i] = (int16_t)lroundf(32767.0f * sinf(2.0f * (float)M_PI * i / 1024.0f));
}

void configurar_pwm() {
  const uint32_t clk = clock_get_hz(clk_sys);

  // Portadora: GPIO15 es canal B de su slice; tope 255 -> 8 bits
  gpio_set_function(PIN_PWM, GPIO_FUNC_PWM);
  slice_portadora = pwm_gpio_to_slice_num(PIN_PWM);
  pwm_config c = pwm_get_default_config();
  pwm_config_set_clkdiv(&c, 1.0f);
  pwm_config_set_wrap(&c, 255);
  pwm_init(slice_portadora, &c, true);
  pwm_set_chan_level(slice_portadora, PWM_CHAN_B, 128);
  f_portadora = (double)clk / 256.0;

  // Reloj de muestreo: divisor 2.5 y tope = clk / (2.5 * fs)
  if (slice_reloj == slice_portadora) slice_reloj = 1;
  const uint32_t cuentas = (uint32_t)(((uint64_t)clk * 2) / (5ULL * FS_HZ));
  fs_real = (double)clk / (2.5 * (double)cuentas);
  pwm_config r = pwm_get_default_config();
  pwm_config_set_clkdiv(&r, 2.5f);
  pwm_config_set_wrap(&r, cuentas - 1);
  pwm_init(slice_reloj, &r, false);
  pwm_clear_irq(slice_reloj);
  pwm_set_irq_enabled(slice_reloj, true);
  irq_set_exclusive_handler(PWM_IRQ_WRAP, on_muestra);
  irq_set_enabled(PWM_IRQ_WRAP, true);
}

void cargar_modo(uint8_t n) {
  const Modo &m = MODOS[n - 1];
  float suma = 0.0f;
  for (uint8_t i = 0; i < m.nt; i++) suma += m.a[i];

  uint32_t nuevo_inc[MAX_TONOS] = {0};
  int32_t  nueva_amp[MAX_TONOS] = {0};
  for (uint8_t i = 0; i < m.nt; i++) {
    nuevo_inc[i] = (uint32_t)((double)m.f[i] * 4294967296.0 / fs_real + 0.5);
    nueva_amp[i] = (int32_t)lroundf(32767.0f * NIVEL_PICO * m.a[i] / suma);
  }

  noInterrupts();                          // cambio atomico respecto a la IRQ
  nt = m.nt;
  for (uint8_t i = 0; i < MAX_TONOS; i++) { inc[i] = nuevo_inc[i]; amp[i] = nueva_amp[i]; }
  interrupts();
  modo_actual = n;
}

// ------------------------------------------------------------------- serial
void procesar(const char *s) {
  if (strncmp(s, "MODO ", 5) == 0) {
    int n = atoi(s + 5);
    if (n >= 1 && n <= 3) { cargar_modo((uint8_t)n); Serial.print("OK "); Serial.println(n); }
    else Serial.println("ERR modo");
  } else if (strcmp(s, "CAPTURA") == 0) {
    idx_cap = 0; captura_lista = false; capturando = true;
  } else if (strcmp(s, "INFO") == 0) {
    Serial.print("INFO modo=");      Serial.print(modo_actual);
    Serial.print(" fs_hz=");         Serial.print(fs_real, 3);
    Serial.print(" portadora_hz=");  Serial.print(f_portadora, 1);
    Serial.print(" nivel_pico=");    Serial.println(NIVEL_PICO, 2);
  } else {
    Serial.println("ERR comando");
  }
}

void setup() {
  Serial.begin(115200);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) { delay(10); }

  construir_tabla();
  adc_init();
  adc_gpio_init(PIN_ADC);
  adc_select_input(0);

  configurar_pwm();
  cargar_modo(1);                          // modo por defecto
  pwm_set_enabled(slice_reloj, true);      // arranca el reloj de muestreo
}

void loop() {
  static char    linea[32];
  static uint8_t nlin = 0;

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') { linea[nlin] = 0; if (nlin) procesar(linea); nlin = 0; }
    else if (nlin < sizeof(linea) - 1) linea[nlin++] = c;
  }

  if (captura_lista) {
    captura_lista = false;
    Serial.println("CAP_INICIO");
    for (uint16_t i = 0; i < N_CAPTURA; i++) Serial.println(cap[i]);
    Serial.println("CAP_FIN");
  }
}

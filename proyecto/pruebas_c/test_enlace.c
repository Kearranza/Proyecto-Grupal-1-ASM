// Enlace nodo 1 -> nodo 3 en la computadora, sin placas (Luis).
// La Pico W genera FUENTE SIM (modo 3: tonos utiles + 3500 Hz), procesa cada salto
// y arma las tramas en el mismo orden que nodo1_pico_w.ino. Los bytes pasan por el
// lector de trama.c y se interpretan como en nodos23_esp32.ino. El nodo 3 filtra con
// el diagnostico recibido. Verifica que los dos lados calzan antes de ir a placa.
#include <string.h>
#include "prueba.h"

#define SALTOS 64
static nodo1_t pico;
static espectral_t esp;
static uint8_t cable[SALTOS * 2 * TRAMA_MAX_BYTES / 4];   // bytes "en el aire"
static float salida[SALTOS * SALTO_MAX], entrada[SALTOS * SALTO_MAX];

static float amplitud(const float *x, int desde, int largo, float f) {
    double c = 0.0, s = 0.0;
    for (int i = desde; i < desde + largo; i++) {
        c += x[i] * cos(2.0 * M_PI * f * i / FS_HZ);
        s += x[i] * sin(2.0 * M_PI * f * i / FS_HZ);
    }
    return (float)(2.0 * hypot(c, s) / largo);
}

int main(void) {
    printf("\n== enlace nodo 1 -> nodo 3 ==\n");
    const int n = N_POR_DEFECTO, salto = n / 2;

    // --- Pico W: genera, procesa y arma tramas (DIAGNOSTICO antes que MUESTRAS)
    nodo1_iniciar(&pico, n);
    size_t enviados = 0;
    uint16_t seq = 0;
    static int16_t nuevas[SALTO_MAX];
    for (int k = 0; k < SALTOS; k++) {
        nodo1_generar_sim((uint32_t)k * salto, salto, nuevas);
        diagnostico_t d;
        int r = nodo1_procesar_salto(&pico, nuevas, &d);
        if (r == ASM_OK)
            enviados += trama_armar(TRAMA_TIPO_DIAGNOSTICO, seq++, (const uint8_t *)&d, sizeof d,
                                    cable + enviados, sizeof cable - enviados);
        enviados += trama_armar(TRAMA_TIPO_MUESTRAS, seq++, (const uint8_t *)nuevas, salto * 2,
                                cable + enviados, sizeof cable - enviados);
    }
    printf("              %zu bytes por el cable en %.0f ms de senal (%.0f kB/s de %.0f disponibles)\n",
           enviados, 1000.0 * SALTOS * salto / FS_HZ,
           enviados / (1.0 * SALTOS * salto / FS_HZ) / 1000.0, BAUDIOS / 10.0 / 1000.0);
    if (enviados / (1.0 * SALTOS * salto / FS_HZ) < BAUDIOS / 10.0) OK("el flujo cabe en la UART 8N1");
    else FALLA("ancho de banda", "el flujo no cabe en %d baud", BAUDIOS);

    // --- ESP32: lee byte a byte y pega a cada salto el ultimo diagnostico
    lector_trama_t lector;
    static trama_t t;
    trama_lector_iniciar(&lector);
    espectral_iniciar(&esp, n);
    diagnostico_t d = {RUIDO_NINGUNO, 0, 0, 0, 0, 0};
    int saltos = 0, con_diagnostico = 0, tonal_3500 = 0;
    for (size_t i = 0; i < enviados; i++) {
        if (trama_leer_byte(&lector, cable[i], &t) != 1) continue;
        if (t.tipo == TRAMA_TIPO_DIAGNOSTICO && t.largo == sizeof d) {
            memcpy(&d, t.carga, sizeof d);
        } else if (t.tipo == TRAMA_TIPO_MUESTRAS) {
            int m = t.largo / 2;
            for (int j = 0; j < m; j++) {
                int16_t v;
                memcpy(&v, t.carga + 2 * j, 2);
                entrada[saltos * m + j] = v / (float)(1 << (ADC_BITS - 1));
            }
            if (d.n_bloque == 2 * m) con_diagnostico++;   // condicion del nodo 2 en el ESP32
            if (d.tipo == RUIDO_TONAL && fabsf(d.frecuencia_hz - 3500.0f) < (float)FS_HZ / n) tonal_3500++;
            espectral_procesar_salto(&esp, entrada + saltos * m, salida + saltos * m, &d, FS_HZ);
            saltos++;
        }
    }
    if (lector.errores_crc == 0 && lector.bloques_perdidos == 0 && saltos == SALTOS) OK("todas las tramas llegan, sin CRC malo ni perdidas");
    else FALLA("enlace", "saltos %d/%d, crc %u, perdidos %u", saltos, SALTOS,
               (unsigned)lector.errores_crc, (unsigned)lector.bloques_perdidos);
    // El primer salto llega sin diagnostico (el bloque todavia no esta lleno)
    if (con_diagnostico == SALTOS - 1) OK("cada salto, salvo el primero, llega con n_bloque = 2 x salto");
    else FALLA("diagnostico por salto", "%d de %d saltos con diagnostico valido", con_diagnostico, SALTOS - 1);
    if (tonal_3500 == SALTOS - 1) OK("el ESP32 recibe tonal en 3500 Hz");
    else FALLA("diagnostico recibido", "%d de %d saltos con tonal 3500 Hz", tonal_3500, SALTOS - 1);

    // --- Resultado: el nodo 3 quita la interferencia que diagnostico el nodo 1
    int desde = 8 * salto, largo = (SALTOS - 12) * salto;
    float aten = 20.0f * log10f(amplitud(entrada, desde, largo, 3500.0f) / (amplitud(salida, desde, largo, 3500.0f) + 1e-9f));
    float cambio = 20.0f * log10f(amplitud(entrada, desde, largo, 440.0f) / (amplitud(salida, desde, largo, 440.0f) + 1e-9f));
    printf("              3500 Hz atenuado %.1f dB, 440 Hz cambia %.2f dB\n", aten, cambio);
    if (aten >= 20.0f && fabsf(cambio) <= 1.0f) OK("nodo 1 + nodo 3: atenua 3500 Hz y conserva 440 Hz");
    else FALLA("cadena completa", "atenuacion %.1f dB, cambio util %.2f dB", aten, cambio);
    return resumen("enlace");
}

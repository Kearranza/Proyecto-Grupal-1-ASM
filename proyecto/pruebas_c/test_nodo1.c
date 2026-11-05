// Contrato de nodo1.c (Luis): el procesamiento completo del nodo 1 con senales que
// pasan por un ADC de 12 bits simulado, para N = 512, 1024 y 2048.
#include <string.h>
#include "prueba.h"

#define LARGO (8 * N_MAX)
static float senal[LARGO];
static int16_t cuentas[LARGO];
static nodo1_t p;

static double gauss(void) {   // Box-Muller
    double u = (rand() + 1.0) / (RAND_MAX + 2.0), v = (rand() + 1.0) / (RAND_MAX + 2.0);
    return sqrt(-2.0 * log(u)) * cos(2.0 * M_PI * v);
}

// Tres tonos utiles; 'ruido' elige que se les suma
static void armar(const char *ruido) {
    double potencia_util = (1.0 + 0.55 * 0.55 + 0.3 * 0.3) / 2.0;
    for (int i = 0; i < LARGO; i++) {
        double t = (double)i / FS_HZ;
        double x = sin(2 * M_PI * 440 * t) + 0.55 * sin(2 * M_PI * 880 * t + 0.6) + 0.3 * sin(2 * M_PI * 1320 * t - 0.9);
        if (!strcmp(ruido, "blanco")) x += sqrt(potencia_util) * gauss();
        // Impulsos en instantes al azar (en promedio 1 cada 100 muestras, para que todo
        // bloque de 512 tenga varios) y de signo
        // al azar: un tren periodico seria un tono en el espectro, no ruido impulsivo
        if (!strcmp(ruido, "impulsivo") && rand() % 100 == 0) x += rand() % 2 ? 3.0 : -3.0;
        senal[i] = (float)x;
    }
    float pico = 0.0f;
    for (int i = 0; i < LARGO; i++) pico = fmaxf(pico, fabsf(senal[i]));
    for (int i = 0; i < LARGO; i++) cuentas[i] = (int16_t)lroundf(senal[i] / pico * 0.8f * 2047.0f);
}

// Pasa la senal por saltos y devuelve el ultimo diagnostico. Cuenta los arranques.
static int correr(int n, diagnostico_t *d, int *arranques) {
    if (nodo1_iniciar(&p, n) != ASM_OK) return ASM_ERROR;
    int r = ASM_ERROR;
    *arranques = 0;
    for (int i = 0; i + n / 2 <= LARGO; i += n / 2) {
        r = nodo1_procesar_salto(&p, cuentas + i, d);
        if (r == NODO1_ARRANCANDO) (*arranques)++;
        else if (r != ASM_OK) return r;
    }
    return r;
}

static void probar(const char *nombre, int n, int tipo, float frecuencia) {
    diagnostico_t d;
    int arranques, r = correr(n, &d, &arranques);
    if (es_pendiente(r, nombre, "Luis")) return;
    if (r != ASM_OK) { FALLA(nombre, "devolvio %d", r); return; }
    if (arranques != 1) { FALLA(nombre, "%d saltos de arranque, se esperaba 1", arranques); return; }
    if (d.tipo != tipo) { FALLA(nombre, "tipo %d, se esperaba %d", d.tipo, tipo); return; }
    if (d.n_bloque != n) { FALLA(nombre, "n_bloque %d, el ESP32 espera %d", d.n_bloque, n); return; }
    if (frecuencia > 0.0f && fabsf(d.frecuencia_hz - frecuencia) > (float)FS_HZ / n) {
        FALLA(nombre, "frecuencia %.1f Hz, se esperaba %.1f", d.frecuencia_hz, frecuencia);
        return;
    }
    OK(nombre);
}

int main(void) {
    printf("\n== nodo1.c ==\n");
    srand(3);
    if (nodo1_iniciar(&p, 1000) == ASM_ERROR && nodo1_iniciar(&p, 4096) == ASM_ERROR) OK("rechaza N invalido");
    else FALLA("N invalido", "deberia devolver ASM_ERROR");

    int ns[] = {512, 1024, N_MAX};
    char nombre[80];
    for (int i = 0; i < 3; i++) {
        int n = ns[i];
        nodo1_generar_sim(0, LARGO, cuentas);
        snprintf(nombre, sizeof nombre, "FUENTE SIM (modo 3) -> tonal 3500 Hz, N = %d", n);
        probar(nombre, n, RUIDO_TONAL, 3500.0f);
        armar("ninguno");
        snprintf(nombre, sizeof nombre, "solo tonos utiles -> ninguno, N = %d", n);
        probar(nombre, n, RUIDO_NINGUNO, 0.0f);
        armar("blanco");
        snprintf(nombre, sizeof nombre, "ruido blanco a 0 dB -> blanco, N = %d", n);
        probar(nombre, n, RUIDO_BLANCO, 0.0f);
        armar("impulsivo");
        snprintf(nombre, sizeof nombre, "impulsos -> impulsivo, N = %d", n);
        probar(nombre, n, RUIDO_IMPULSIVO, 0.0f);
    }

    // La fuente simulada es continua entre saltos (no hay saltos de fase)
    int16_t a[600], b[600];
    nodo1_generar_sim(0, 600, a);
    nodo1_generar_sim(300, 300, b);
    if (!memcmp(a + 300, b, 300 * sizeof(int16_t))) OK("FUENTE SIM continua entre saltos");
    else FALLA("FUENTE SIM continua", "la muestra 300 no coincide al generar por partes");

    // Ventana deslizante: al final, el bloque son las ultimas n muestras
    diagnostico_t d;
    int arranques;
    nodo1_generar_sim(0, LARGO, cuentas);
    correr(1024, &d, &arranques);
    int fin = (LARGO / 512) * 512;
    if (fabsf(p.bloque[1023] - cuentas[fin - 1] / 2048.0f) < 1e-6f && fabsf(p.bloque[0] - cuentas[fin - 1024] / 2048.0f) < 1e-6f)
        OK("el bloque son las ultimas N muestras");
    else FALLA("ventana deslizante", "el bloque no coincide con las ultimas N muestras");
    return resumen("nodo1.c");
}

// Contrato de espectral.c (Gretchell), con propiedades en vez de muestras exactas:
//  1. Sin ruido que quitar, la salida es la entrada retrasada (reconstruccion perfecta).
//  2. Con interferencia tonal, la atenua sin dañar el tono util.
#include "prueba.h"

#define LARGO 16384
// Criterios PROVISIONALES: Gretchell los ajusta y los justifica en el paper
#define ERROR_MAX_IDENTIDAD 1e-3f
#define ATEN_MIN_DB 20.0f
#define PERDIDA_UTIL_MAX_DB 1.0f

static float entrada[LARGO], salida[LARGO];
static espectral_t e;

static int procesar(int n, const diagnostico_t *d) {
    int salto = n / 2, r = espectral_iniciar(&e, n);
    for (int i = 0; r == ASM_OK && i + salto <= LARGO; i += salto)
        r = espectral_procesar_salto(&e, entrada + i, salida + i, d, FS_HZ);
    return r;
}

// Amplitud de un seno de frecuencia f en x[desde..desde+largo) por correlacion
static float amplitud(const float *x, int desde, int largo, float f) {
    double c = 0.0, s = 0.0;
    for (int i = desde; i < desde + largo; i++) {
        c += x[i] * cos(2.0 * M_PI * f * i / FS_HZ);
        s += x[i] * sin(2.0 * M_PI * f * i / FS_HZ);
    }
    return (float)(2.0 * hypot(c, s) / largo);
}

static void probar_identidad(int n) {
    char nombre[64];
    snprintf(nombre, sizeof nombre, "reconstruccion perfecta sin ruido, n = %d", n);
    for (int i = 0; i < LARGO; i++) entrada[i] = 0.5f * sinf(2.0f * (float)M_PI * 440.0f * i / FS_HZ);
    diagnostico_t d = {RUIDO_NINGUNO, 0, (uint16_t)n, 0.0f, 0.0f, 0.0f};
    int r = procesar(n, &d);
    if (es_pendiente(r, nombre, "Gretchell")) return;
    if (r != ASM_OK) { FALLA(nombre, "devolvio %d", r); return; }
    // Busca el retraso que mejor alinea la salida con la entrada
    float mejor = 1e9f;
    int retraso = 0;
    for (int dly = 0; dly <= n; dly++) {
        float err = 0.0f;
        for (int i = 2 * n; i < LARGO - n; i++) err = fmaxf(err, fabsf(salida[i] - entrada[i - dly]));
        if (err < mejor) { mejor = err; retraso = dly; }
    }
    if (mejor <= ERROR_MAX_IDENTIDAD) { OK(nombre); printf("              retraso = %d muestras\n", retraso); }
    else FALLA(nombre, "error %.2e con el mejor retraso (%d)", mejor, retraso);
}

static void probar_tonal(void) {
    const char *nombre = "atenua 3500 Hz y conserva 440 Hz";
    for (int i = 0; i < LARGO; i++)
        entrada[i] = 0.4f * sinf(2.0f * (float)M_PI * 440.0f * i / FS_HZ)
                   + 0.4f * sinf(2.0f * (float)M_PI * 3500.0f * i / FS_HZ);
    diagnostico_t d = {RUIDO_TONAL, 0, N_POR_DEFECTO, 3500.0f, 0.0f, 0.0f};
    int r = procesar(N_POR_DEFECTO, &d);
    if (es_pendiente(r, nombre, "Gretchell")) return;
    int desde = 4 * N_POR_DEFECTO, largo = LARGO - desde - N_POR_DEFECTO;
    float aten = 20.0f * log10f(amplitud(entrada, desde, largo, 3500.0f) / (amplitud(salida, desde, largo, 3500.0f) + 1e-9f));
    float perdida = 20.0f * log10f(amplitud(entrada, desde, largo, 440.0f) / (amplitud(salida, desde, largo, 440.0f) + 1e-9f));
    if (aten >= ATEN_MIN_DB && fabsf(perdida) <= PERDIDA_UTIL_MAX_DB) OK(nombre);
    else FALLA(nombre, "atenuacion %.1f dB (min %.0f), cambio en 440 Hz %.2f dB (max %.1f)",
               aten, ATEN_MIN_DB, perdida, PERDIDA_UTIL_MAX_DB);
}

int main(void) {
    printf("\n== espectral.c ==\n");
    probar_identidad(256);
    probar_identidad(N_POR_DEFECTO);
    probar_tonal();
    return resumen("espectral.c");
}

// Contrato de fft.c (Gretchell): compara contra una DFT por definicion en double.
#include "prueba.h"

#define TOL_REL 1e-4   // float32 con N = 2048: error relativo esperado ~1e-6..1e-5

static double azar(void) { return 2.0 * rand() / RAND_MAX - 1.0; }

static int probar_n(int n) {
    static float re[N_MAX], im[N_MAX], x[N_MAX];
    char nombre[64];
    snprintf(nombre, sizeof nombre, "fft contra DFT, n = %d", n);
    for (int i = 0; i < n; i++) { x[i] = (float)azar(); re[i] = x[i]; im[i] = 0.0f; }
    int r = fft(re, im, n);
    if (es_pendiente(r, nombre, "Gretchell")) return 0;
    if (r != ASM_OK) { FALLA(nombre, "devolvio %d", r); return 0; }

    double err_max = 0.0, mag_max = 0.0;
    for (int k = 0; k < n; k++) {
        double sr = 0.0, si = 0.0;
        for (int t = 0; t < n; t++) {
            double ang = -2.0 * M_PI * (double)k * t / n;
            sr += x[t] * cos(ang);
            si += x[t] * sin(ang);
        }
        double e = hypot(re[k] - sr, im[k] - si);
        if (e > err_max) err_max = e;
        if (hypot(sr, si) > mag_max) mag_max = hypot(sr, si);
    }
    if (err_max / mag_max > TOL_REL) FALLA(nombre, "error relativo %.2e", err_max / mag_max);
    else OK(nombre);

    snprintf(nombre, sizeof nombre, "ifft(fft(x)) == x, n = %d", n);
    r = ifft(re, im, n);
    if (es_pendiente(r, nombre, "Gretchell")) return 0;
    double e_ida = 0.0;
    for (int i = 0; i < n; i++) e_ida = fmax(e_ida, fabs(re[i] - x[i]) + fabs(im[i]));
    if (e_ida > 1e-4) FALLA(nombre, "error maximo %.2e", e_ida);
    else OK(nombre);
    return 0;
}

int main(void) {
    printf("\n== fft.c ==\n");
    srand(1);
    int ns[] = {8, 256, 1024, N_MAX};
    for (size_t i = 0; i < sizeof ns / sizeof ns[0]; i++) probar_n(ns[i]);

    float re[16] = {0}, im[16] = {0};
    int r = fft(re, im, 12);
    if (!es_pendiente(r, "n que no es potencia de dos", "Gretchell")) {
        if (r == ASM_ERROR) OK("n que no es potencia de dos");
        else FALLA("n que no es potencia de dos", "deberia devolver ASM_ERROR");
    }
    return resumen("fft.c");
}

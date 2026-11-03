// Contrato de metricas.c (Gretchell): mismos valores que src/metricas.py.
#include "prueba.h"

#define TOL_REL 1e-3

int main(void) {
    printf("\n== metricas.c ==\n");
    const char *nombre = "MSE, SNR y delta SNR igual que src/metricas.py";
    FILE *f = abrir_referencia("datos/metricas.txt", nombre);
    if (!f) return resumen("metricas.c");
    int n;
    static float s[8192], x[8192], y[8192];
    float esperado[4];
    int valido = fscanf(f, "%d", &n) == 1 && n <= 8192;
    for (int i = 0; valido && i < n; i++) valido = fscanf(f, "%f %f %f", &s[i], &x[i], &y[i]) == 3;
    for (int i = 0; valido && i < 4; i++) valido = fscanf(f, "%f", &esperado[i]) == 1;
    fclose(f);
    if (!valido) { FALLA(nombre, "archivo invalido"); return resumen("metricas.c"); }

    metricas_t m;
    metricas_reiniciar(&m);
    // Se acumula por saltos de 512, como lo hara el nodo 3
    int r = ASM_OK;
    for (int i = 0; i < n && r == ASM_OK; i += 512) r = metricas_acumular(&m, s + i, x + i, y + i, 512);
    if (es_pendiente(r, nombre, "Gretchell")) return resumen("metricas.c");
    resultado_metricas_t res;
    r = metricas_calcular(&m, &res);
    if (es_pendiente(r, nombre, "Gretchell")) return resumen("metricas.c");

    float obtenido[4] = {res.mse, res.snr_entrada_db, res.snr_salida_db, res.delta_snr_db};
    const char *nombres[4] = {"MSE", "SNR entrada", "SNR salida", "delta SNR"};
    int bien = 1;
    for (int i = 0; i < 4; i++) {
        if (fabsf(obtenido[i] - esperado[i]) > TOL_REL * fmaxf(1.0f, fabsf(esperado[i]))) {
            FALLA(nombre, "%s = %g, se esperaba %g", nombres[i], obtenido[i], esperado[i]);
            bien = 0;
        }
    }
    if (bien) OK(nombre);
    return resumen("metricas.c");
}

// Contrato de iir.c (Santiago).
//  1. iir_cargar_sos + iir_procesar reproducen scipy.signal.sosfilt (datos/iir.txt).
//  2. iir_disenar con un diagnostico tonal atenua la interferencia y respeta la banda util.
#include "prueba.h"

#define TOL_SOSFILT 1e-3     // float32 contra double de scipy
// Criterios PROVISIONALES: Santiago los ajusta y los justifica en el paper
#define ATEN_MIN_DB 20.0f    // atenuacion minima en la frecuencia interferente
#define RIZO_MAX_DB 1.0f     // variacion maxima en 440, 880 y 1320 Hz

static float x[8192], y[8192];

// Amplitud de salida en estado estable para un seno de frecuencia f
static float ganancia_db(iir_t *f, float frec) {
    const int n = 8000;
    static float e[8000], s[8000];
    for (int i = 0; i < n; i++) e[i] = sinf(2.0f * (float)M_PI * frec * i / FS_HZ);
    iir_reiniciar_estado(f);
    iir_procesar(f, e, s, n);
    float pico = 0.0f;
    for (int i = n / 2; i < n; i++) pico = fmaxf(pico, fabsf(s[i]));
    return 20.0f * log10f(pico + 1e-12f);
}

static void probar_sosfilt(void) {
    const char *nombre = "iir_procesar igual a scipy sosfilt";
    FILE *f = abrir_referencia("datos/iir.txt", nombre);
    if (!f) return;
    int etapas, n;
    float sos[IIR_MAX_ETAPAS * 6];
    if (fscanf(f, "%d", &etapas) != 1 || etapas > IIR_MAX_ETAPAS) { FALLA(nombre, "archivo invalido"); fclose(f); return; }
    for (int i = 0; i < etapas * 6; i++) if (fscanf(f, "%f", &sos[i]) != 1) { FALLA(nombre, "archivo invalido"); fclose(f); return; }
    if (fscanf(f, "%d", &n) != 1 || n > 8192) { FALLA(nombre, "archivo invalido"); fclose(f); return; }
    static float esperado[8192];
    for (int i = 0; i < n; i++) if (fscanf(f, "%f %f", &x[i], &esperado[i]) != 2) { FALLA(nombre, "archivo invalido"); fclose(f); return; }
    fclose(f);

    iir_t filtro;
    int r = iir_cargar_sos(&filtro, sos, etapas);
    if (es_pendiente(r, nombre, "Santiago")) return;
    iir_reiniciar_estado(&filtro);
    r = iir_procesar(&filtro, x, y, n);
    if (es_pendiente(r, nombre, "Santiago")) return;
    float err = 0.0f;
    for (int i = 0; i < n; i++) err = fmaxf(err, fabsf(y[i] - esperado[i]));
    if (err <= TOL_SOSFILT) OK(nombre);
    else FALLA(nombre, "error maximo %.2e", err);
}

static void probar_diseno(void) {
    const char *nombre = "iir_disenar con interferencia tonal de 3500 Hz";
    diagnostico_t d = {RUIDO_TONAL, 0, N_POR_DEFECTO, 3500.0f, 0.0f, 0.0f};
    iir_t filtro;
    int r = iir_disenar(&filtro, &d, FS_HZ);
    if (es_pendiente(r, nombre, "Santiago")) return;
    if (r != ASM_OK) { FALLA(nombre, "devolvio %d", r); return; }
    float aten = -ganancia_db(&filtro, 3500.0f);
    float utiles[] = {440.0f, 880.0f, 1320.0f};
    float rizo = 0.0f;
    for (int i = 0; i < 3; i++) rizo = fmaxf(rizo, fabsf(ganancia_db(&filtro, utiles[i])));
    if (aten >= ATEN_MIN_DB && rizo <= RIZO_MAX_DB) OK(nombre);
    else FALLA(nombre, "atenuacion %.1f dB (min %.0f), rizo util %.2f dB (max %.1f)",
               aten, ATEN_MIN_DB, rizo, RIZO_MAX_DB);
}

int main(void) {
    printf("\n== iir.c ==\n");
    probar_sosfilt();
    probar_diseno();
    return resumen("iir.c");
}

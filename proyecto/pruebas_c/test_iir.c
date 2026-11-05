// Contrato de iir.c (Santiago).
//  1. iir_cargar_sos + iir_procesar reproducen scipy.signal.sosfilt (datos/iir.txt).
//  2. iir_disenar con un diagnostico tonal atenua la interferencia y respeta la banda util.
//  3. Zumbido de red (60 Hz y armonicos), banda, ruidos sin filtro temporal y errores.
#include "prueba.h"

#define TOL_SOSFILT 1e-3     // float32 contra double de scipy (el error medido es ~1e-6)

// ---------------------------------------------------------------------------
// Criterios de aceptacion de iir_disenar (antes PROVISIONALES; justificados en
// paper/secciones/4_temporal.tex):
//
// ATEN_MIN_DB = 20: la frecuencia que da el nodo 1 sale de un bin de la FFT y
//   puede estar corrida hasta medio bin (FS_HZ / (2 N) = 7,8 Hz con N = 1024).
//   Por eso la atenuacion se mide en la frecuencia estimada y tambien en
//   +-medio bin, y se exige la peor de las tres. 20 dB reducen la amplitud de la
//   interferencia diez veces: con la SNR de entrada de P1 (2 dB) llevan la salida
//   a unos 22 dB. El diseno apunta a 25 dB (ATEN_DISENO_DB en iir.c), asi que 20 dB
//   deja 5 dB de margen por redondeo a float32.
//   Un notch de segundo orden no alcanza los 40 dB que tenia el eliptico de la
//   Tarea 1, pero este se recalcula sin SciPy y sigue la frecuencia que mande el nodo 1.
// RIZO_MAX_DB = 1: es el rizo de paso del diseno de la Tarea 1 (gpass = 1 dB), medido
//   en los tres tonos utiles (440, 880 y 1320 Hz).
// ---------------------------------------------------------------------------
#define ATEN_MIN_DB 20.0f
#define RIZO_MAX_DB 1.0f
#define ATEN_RED_MIN_DB 20.0f    // en las frecuencias exactas de 60 Hz y sus armonicos
#define ATEN_BORDE_BANDA_DB 5.0f // dos notches de -3 dB dan 6 dB en los bordes de la banda
#define RIZO_BANDA_MAX_DB 3.0f   // 440 Hz esta a solo 360 Hz del borde de 800 Hz y las faldas de
                                 // un notch son suaves (se miden ~2,2 dB); un eliptico lo evitaria

#define MEDIO_BIN_HZ (FS_HZ / (2.0f * N_POR_DEFECTO))

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
    // peor caso: la frecuencia estimada y medio bin a cada lado
    float aten = -ganancia_db(&filtro, 3500.0f);
    aten = fminf(aten, -ganancia_db(&filtro, 3500.0f - MEDIO_BIN_HZ));
    aten = fminf(aten, -ganancia_db(&filtro, 3500.0f + MEDIO_BIN_HZ));
    float utiles[] = {440.0f, 880.0f, 1320.0f};
    float rizo = 0.0f;
    for (int i = 0; i < 3; i++) rizo = fmaxf(rizo, fabsf(ganancia_db(&filtro, utiles[i])));
    if (aten >= ATEN_MIN_DB && rizo <= RIZO_MAX_DB) OK(nombre);
    else FALLA(nombre, "atenuacion %.1f dB (min %.0f), rizo util %.2f dB (max %.1f)",
               aten, ATEN_MIN_DB, rizo, RIZO_MAX_DB);
    printf("      (peor atenuacion a +-medio bin: %.1f dB; rizo en los tonos utiles: %.3f dB)\n", aten, rizo);
}

static void probar_zumbido_red(void) {
    const char *nombre = "iir_disenar con zumbido de 60 Hz y armonicos";
    diagnostico_t d = {RUIDO_TONAL, 0, N_POR_DEFECTO, 60.0f, 0.0f, 0.0f};
    iir_t filtro;
    int r = iir_disenar(&filtro, &d, FS_HZ);
    if (es_pendiente(r, nombre, "Santiago")) return;
    if (r != ASM_OK) { FALLA(nombre, "devolvio %d", r); return; }
    float aten = 1e9f;
    for (int k = 1; k <= 5; k++) aten = fminf(aten, -ganancia_db(&filtro, 60.0f * k));
    float rizo = fabsf(ganancia_db(&filtro, 440.0f));
    if (aten >= ATEN_RED_MIN_DB && rizo <= RIZO_MAX_DB) OK(nombre);
    else FALLA(nombre, "atenuacion %.1f dB (min %.0f), rizo en 440 Hz %.2f dB (max %.1f)",
               aten, ATEN_RED_MIN_DB, rizo, RIZO_MAX_DB);
    printf("      (%d etapas; peor atenuacion en 60..300 Hz: %.1f dB; rizo en 440 Hz: %.3f dB)\n",
           filtro.n_etapas, aten, rizo);
}

static void probar_banda(void) {
    const char *nombre = "iir_disenar con ruido de banda de 800 a 1500 Hz";
    diagnostico_t d = {RUIDO_BANDA, 0, N_POR_DEFECTO, 1150.0f, 700.0f, 0.0f};
    iir_t filtro;
    int r = iir_disenar(&filtro, &d, FS_HZ);
    if (es_pendiente(r, nombre, "Santiago")) return;
    if (r != ASM_OK) { FALLA(nombre, "devolvio %d", r); return; }
    float borde = fminf(-ganancia_db(&filtro, 800.0f), -ganancia_db(&filtro, 1500.0f));
    float fuera = fabsf(ganancia_db(&filtro, 440.0f));
    if (borde >= ATEN_BORDE_BANDA_DB && fuera <= RIZO_BANDA_MAX_DB) OK(nombre);
    else FALLA(nombre, "atenuacion en los bordes %.1f dB (min %.0f), rizo en 440 Hz %.2f dB (max %.1f)",
               borde, ATEN_BORDE_BANDA_DB, fuera, RIZO_BANDA_MAX_DB);
    printf("      (atenuacion en los bordes: %.1f dB; en el centro: %.1f dB; rizo en 440 Hz: %.3f dB)\n",
           borde, -ganancia_db(&filtro, 1150.0f), fuera);
}

static int estable(const iir_t *f) {   // Jury, igual que iir.c, repetido para no depender de el
    for (int i = 0; i < f->n_etapas; i++) {
        const biquad_t *s = &f->etapas[i];
        if (!(fabsf(s->a2) < 1.0f && fabsf(s->a1) < 1.0f + s->a2)) return 0;
    }
    return 1;
}

static void probar_estabilidad(void) {
    const char *nombre = "iir_disenar siempre entrega un filtro estable";
    const float frecs[] = {20.0f, 60.0f, 440.0f, 1000.0f, 3500.0f, 7000.0f};
    const int tamanos[] = {256, 512, 1024, 2048};
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < 4; j++) {
            diagnostico_t d = {RUIDO_TONAL, 0, (uint16_t)tamanos[j], frecs[i], 0.0f, 0.0f};
            iir_t filtro;
            int r = iir_disenar(&filtro, &d, FS_HZ);
            if (es_pendiente(r, nombre, "Santiago")) return;
            if (r != ASM_OK || !estable(&filtro)) {
                FALLA(nombre, "f0 = %.0f Hz, N = %d: devolvio %d o es inestable", frecs[i], tamanos[j], r);
                return;
            }
        }
    OK(nombre);
}

static void probar_sin_filtro(void) {
    const char *nombre = "ruido blanco, impulsivo o ninguno: pasa directo";
    const uint8_t tipos[] = {RUIDO_NINGUNO, RUIDO_BLANCO, RUIDO_IMPULSIVO};
    for (int i = 0; i < 128; i++) x[i] = sinf(0.37f * i) * 0.5f;
    for (int t = 0; t < 3; t++) {
        diagnostico_t d = {tipos[t], 0, N_POR_DEFECTO, 0.0f, 0.0f, 0.0f};
        iir_t filtro;
        int r = iir_disenar(&filtro, &d, FS_HZ);
        if (es_pendiente(r, nombre, "Santiago")) return;
        if (r != ASM_OK) { FALLA(nombre, "tipo %d devolvio %d", tipos[t], r); return; }
        iir_procesar(&filtro, x, y, 128);
        for (int i = 0; i < 128; i++)
            if (fabsf(y[i] - x[i]) > 1e-6f) { FALLA(nombre, "tipo %d altero la senal", tipos[t]); return; }
    }
    OK(nombre);
}

static void probar_errores(void) {
    const char *nombre = "iir_disenar rechaza entradas invalidas";
    iir_t filtro;
    diagnostico_t arriba = {RUIDO_TONAL, 0, N_POR_DEFECTO, 9000.0f, 0.0f, 0.0f};   // sobre Nyquist
    diagnostico_t cero = {RUIDO_TONAL, 0, N_POR_DEFECTO, 0.0f, 0.0f, 0.0f};
    diagnostico_t raro = {99, 0, N_POR_DEFECTO, 1000.0f, 0.0f, 0.0f};
    int r = iir_disenar(&filtro, &arriba, FS_HZ);
    if (es_pendiente(r, nombre, "Santiago")) return;
    if (r != ASM_ERROR || iir_disenar(&filtro, &cero, FS_HZ) != ASM_ERROR ||
        iir_disenar(&filtro, &raro, FS_HZ) != ASM_ERROR ||
        iir_disenar(NULL, &cero, FS_HZ) != ASM_ERROR ||
        iir_disenar(&filtro, NULL, FS_HZ) != ASM_ERROR) {
        FALLA(nombre, "alguna entrada invalida no devolvio ASM_ERROR");
        return;
    }
    // una etapa inestable no se carga
    const float sos_malo[6] = {1, 0, 0, 1, 0, 1.5f};   // polos fuera del circulo unitario
    if (iir_cargar_sos(&filtro, sos_malo, 1) != ASM_ERROR) { FALLA(nombre, "cargo un filtro inestable"); return; }
    OK(nombre);
}

int main(void) {
    printf("\n== iir.c ==\n");
    probar_sosfilt();
    probar_diseno();
    probar_zumbido_red();
    probar_banda();
    probar_estabilidad();
    probar_sin_filtro();
    probar_errores();
    return resumen("iir.c");
}
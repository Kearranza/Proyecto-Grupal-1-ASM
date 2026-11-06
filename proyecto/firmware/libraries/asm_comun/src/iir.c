// Nodo 2 temporal (Santiago). Base: src/filtros_temporales.py de la Tarea 1
// (filtrar_sos_manual). Validar con pruebas_c/test_iir.c contra scipy.
//
// Forma de cada etapa (directa II transpuesta, a0 = 1):
//     y    = b0*x + z1
//     z1   = b1*x - a1*y + z2
//     z2   = b2*x - a2*y
#include <math.h>
#include <string.h>
#include "config.h"
#include "iir.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ATEN_DISENO_DB   25.0
#define ANCHO_MIN_HZ      2.0     // no estrechar mas: la estimacion nunca es tan precisa
#define ANCHO_MAX_REL     0.5     // el ancho de un notch tonal no pasa de 0,5 f0 (en 60 Hz no se come el DC)
#define F_RED_MAX_HZ    100.0f    // por debajo, se asume red electrica: se anulan armonicos
#define ARMONICOS_MAX     5
#define FRAC_NYQUIST      0.45    // no se disenan notches por encima de 0,45 fs
#define F_MIN_HZ          5.0f

void iir_reiniciar_estado(iir_t *f) {
    for (int i = 0; i < f->n_etapas; i++) {
        f->etapas[i].z1 = 0.0f;
        f->etapas[i].z2 = 0.0f;
    }
}

// Estabilidad de z^2 + a1 z + a2 (criterio de Jury): |a2| < 1 y |a1| < 1 + a2
static int biquad_estable(const biquad_t *s) {
    return fabsf(s->a2) < 1.0f && fabsf(s->a1) < 1.0f + s->a2;
}

static int todas_estables(const iir_t *f) {
    for (int i = 0; i < f->n_etapas; i++)
        if (!biquad_estable(&f->etapas[i])) return 0;
    return 1;
}

static void filtro_identidad(iir_t *f) {
    memset(f, 0, sizeof *f);
    f->etapas[0].b0 = 1.0f;
    f->n_etapas = 1;
    f->ganancia = 1.0f;
}


static void disenar_notch(biquad_t *s, double f0, double bw, double fs) {
    const double w0 = 2.0 * M_PI * f0 / fs;
    const double r = exp(-M_PI * bw / fs);
    const double c = cos(w0);
    const double g = (f0 > fs / 4.0)
        ? (1.0 - 2.0 * r * c + r * r) / (2.0 - 2.0 * c)      // H(DC) = 1
        : (1.0 + 2.0 * r * c + r * r) / (2.0 + 2.0 * c);     // H(Nyquist) = 1
    s->b0 = (float)g;
    s->b1 = (float)(-2.0 * g * c);
    s->b2 = (float)g;
    s->a1 = (float)(-2.0 * r * c);
    s->a2 = (float)(r * r);
    s->z1 = s->z2 = 0.0f;
}


static double ancho_para_tolerancia(double f_ref, double fs, int n_bloque) {
    const double delta = fs / (2.0 * n_bloque);
    double bw = 2.0 * delta * sqrt(pow(10.0, ATEN_DISENO_DB / 10.0) - 1.0);
    if (bw > ANCHO_MAX_REL * f_ref) bw = ANCHO_MAX_REL * f_ref;
    if (bw < ANCHO_MIN_HZ) bw = ANCHO_MIN_HZ;
    return bw;
}

int iir_disenar(iir_t *f, const diagnostico_t *d, float fs) {
    if (!f || !d || !(fs > 0.0f)) return ASM_ERROR;
    const int n_bloque = d->n_bloque > 0 ? d->n_bloque : N_POR_DEFECTO;

    iir_t nuevo;
    memset(&nuevo, 0, sizeof nuevo);
    nuevo.ganancia = 1.0f;

    switch (d->tipo) {
    case RUIDO_TONAL: {
        // Un notch en la frecuencia estimada. Si es de red electrica (< 100 Hz) se
        // agregan los armonicos, que es lo que trae el zumbido de 60 Hz.
        const float f0 = d->frecuencia_hz;
        if (!(f0 >= F_MIN_HZ && f0 < FRAC_NYQUIST * fs)) return ASM_ERROR;
        const int armonicos = (f0 < F_RED_MAX_HZ) ? ARMONICOS_MAX : 1;
        for (int k = 1; k <= armonicos && nuevo.n_etapas < IIR_MAX_ETAPAS; k++) {
            const double fk = (double)k * f0;
            if (fk >= FRAC_NYQUIST * fs) break;
            disenar_notch(&nuevo.etapas[nuevo.n_etapas++], fk,
                          ancho_para_tolerancia(f0, fs, n_bloque), fs);
        }
        break;
    }
    case RUIDO_BANDA: {
        // Supone que frecuencia_hz es el centro de la banda y ancho_hz su ancho.
        // Dos notches iguales en cascada: atenuan >= 14 dB en la mitad central de
        // la banda y 6 dB en sus bordes (-3 dB cada uno). Las faldas son suaves: el
        // contenido util cercano a la banda tambien pierde nivel. Un filtro de
        // rechazo de banda de verdad (eliptico) queda para cuando el diagnostico
        // entregue los bordes.
        const float f0 = d->frecuencia_hz, bw = d->ancho_hz;
        if (!(f0 >= F_MIN_HZ && f0 < FRAC_NYQUIST * fs && bw > 0.0f)) return ASM_ERROR;
        const double ancho = fmin((double)bw, (double)f0);   // un notch no puede ser mas ancho que su centro
        for (int k = 0; k < 2; k++)
            disenar_notch(&nuevo.etapas[nuevo.n_etapas++], f0, ancho, fs);
        break;
    }
    case RUIDO_NINGUNO:
    case RUIDO_BLANCO:
    case RUIDO_IMPULSIVO:
        // Un filtro temporal fijo no ayuda con estos ruidos: pasa directo y deja
        // el trabajo al nodo 3 (espectral).
        filtro_identidad(&nuevo);
        break;
    default:
        return ASM_ERROR;
    }

    if (!todas_estables(&nuevo)) return ASM_ERROR;
    *f = nuevo;
    return ASM_OK;
}

int iir_cargar_sos(iir_t *f, const float *sos, int n_etapas) {
    // sos en el formato de scipy: por etapa [b0 b1 b2 a0 a1 a2], con a0 = 1
    if (!f || !sos || n_etapas < 1 || n_etapas > IIR_MAX_ETAPAS) return ASM_ERROR;

    iir_t nuevo;
    memset(&nuevo, 0, sizeof nuevo);
    for (int i = 0; i < n_etapas; i++) {
        const float *s = sos + 6 * i;
        if (s[3] == 0.0f) return ASM_ERROR;
        const float inv = 1.0f / s[3];
        biquad_t *e = &nuevo.etapas[i];
        e->b0 = s[0] * inv;
        e->b1 = s[1] * inv;
        e->b2 = s[2] * inv;
        e->a1 = s[4] * inv;
        e->a2 = s[5] * inv;
    }
    nuevo.n_etapas = n_etapas;
    nuevo.ganancia = 1.0f;
    if (!todas_estables(&nuevo)) return ASM_ERROR;
    *f = nuevo;
    return ASM_OK;
}

int iir_procesar(iir_t *f, const float *x, float *y, int n) {
    if (!f || !x || !y || n < 0 || f->n_etapas < 1 || f->n_etapas > IIR_MAX_ETAPAS)
        return ASM_ERROR;
    const float g = f->ganancia;
    for (int i = 0; i < n; i++) {
        float v = x[i];                       // se lee antes de escribir: x e y pueden ser el mismo
        for (int k = 0; k < f->n_etapas; k++) {
            biquad_t *s = &f->etapas[k];
            const float salida = s->b0 * v + s->z1;
            s->z1 = s->b1 * v - s->a1 * salida + s->z2;
            s->z2 = s->b2 * v - s->a2 * salida;
            v = salida;
        }
        y[i] = g * v;
    }
    return ASM_OK;
}
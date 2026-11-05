#include <limits.h>
#include <math.h>
#include <string.h>
#include "config.h"
#include "metricas.h"

static int no_negativo(double x)
{
    return isfinite(x) && x >= 0.0;
}

static int estado_valido(const metricas_t *m)
{
    return m && m->muestras >= 0 && m->bloques_bandas >= 0 && m->bloques_tiempo >= 0 &&
           no_negativo(m->suma_ref2) && no_negativo(m->suma_err_ent2) &&
           no_negativo(m->suma_err_sal2) && no_negativo(m->energia_util_ref) &&
           no_negativo(m->energia_util_sal) && no_negativo(m->energia_interferencia_ent) &&
           no_negativo(m->energia_interferencia_sal) && no_negativo(m->tiempo_total_us) &&
           no_negativo(m->periodo_total_us);
}

static float cociente_db(double numerador, double denominador)
{
    // No agregar un epsilon: un error exactamente nulo representa SNR infinita.
    if (numerador == 0.0 && denominador == 0.0)
        return NAN;
    if (denominador == 0.0)
        return INFINITY;
    if (numerador == 0.0)
        return -INFINITY;
    // Restar logaritmos evita desbordar el cociente antes de calcular los dB.
    return (float)(10.0 * (log10(numerador) - log10(denominador)));
}

void metricas_reiniciar(metricas_t *m)
{
    if (m)
        memset(m, 0, sizeof(*m));
}

int metricas_acumular(metricas_t *m, const float *s, const float *x, const float *y, int n)
{
    if (!estado_valido(m) || !s || !x || !y || n <= 0 || m->muestras > LONG_MAX - n)
        return ASM_ERROR;
    double ref2 = 0.0, err_ent2 = 0.0, err_sal2 = 0.0;
    for (int i = 0; i < n; i++)
    {
        if (!isfinite(s[i]) || !isfinite(x[i]) || !isfinite(y[i]))
            return ASM_ERROR;
        // Restar en double evita desbordar float antes de elevar al cuadrado.
        double referencia = s[i];
        double error_ent = referencia - (double)x[i];
        double error_sal = referencia - (double)y[i];
        ref2 += referencia * referencia;
        err_ent2 += error_ent * error_ent;
        err_sal2 += error_sal * error_sal;
    }
    ref2 += m->suma_ref2;
    err_ent2 += m->suma_err_ent2;
    err_sal2 += m->suma_err_sal2;
    if (!isfinite(ref2) || !isfinite(err_ent2) || !isfinite(err_sal2))
        return ASM_ERROR;
    // Publicar solo al terminar: una muestra invalida no deja un salto parcial.
    m->suma_ref2 = ref2;
    m->suma_err_ent2 = err_ent2;
    m->suma_err_sal2 = err_sal2;
    m->muestras += n;
    return ASM_OK;
}

int metricas_acumular_bandas(metricas_t *m, double util_ref, double util_sal,
                             double interferencia_ent, double interferencia_sal)
{
    if (!estado_valido(m) || m->bloques_bandas == LONG_MAX ||
        !no_negativo(util_ref) || !no_negativo(util_sal) ||
        !no_negativo(interferencia_ent) || !no_negativo(interferencia_sal))
        return ASM_ERROR;
    util_ref += m->energia_util_ref;
    util_sal += m->energia_util_sal;
    interferencia_ent += m->energia_interferencia_ent;
    interferencia_sal += m->energia_interferencia_sal;
    if (!isfinite(util_ref) || !isfinite(util_sal) ||
        !isfinite(interferencia_ent) || !isfinite(interferencia_sal))
        return ASM_ERROR;
    m->energia_util_ref = util_ref;
    m->energia_util_sal = util_sal;
    m->energia_interferencia_ent = interferencia_ent;
    m->energia_interferencia_sal = interferencia_sal;
    m->bloques_bandas++;
    return ASM_OK;
}

int metricas_energia_banda(const float *re, const float *im, int n, float fs,
                           float desde_hz, float hasta_hz, double *energia)
{
    if (!re || !im || !energia || !n_es_valido(n) || !isfinite(fs) || fs <= 0.0f ||
        !isfinite(desde_hz) || !isfinite(hasta_hz) || desde_hz < 0.0f ||
        hasta_hz < desde_hz || hasta_hz > fs / 2.0f)
        return ASM_ERROR;
    double suma = 0.0;
    for (int k = 0; k <= n / 2; k++)
    {
        if (!isfinite(re[k]) || !isfinite(im[k]))
            return ASM_ERROR;
        double frecuencia = (double)k * fs / n;
        if (frecuencia < desde_hz || frecuencia > hasta_hz)
            continue;
        double potencia = (double)re[k] * re[k] + (double)im[k] * im[k];
        // DC y Nyquist no tienen una pareja distinta: no duplicar su energia.
        suma += (k == 0 || k == n / 2) ? potencia : 2.0 * potencia;
    }
    *energia = suma / n;
    return ASM_OK;
}

int metricas_acumular_tiempo(metricas_t *m, uint32_t tiempo_us, uint32_t periodo_us)
{
    if (!estado_valido(m) || !periodo_us || m->bloques_tiempo == LONG_MAX)
        return ASM_ERROR;
    double total = m->tiempo_total_us + tiempo_us;
    double periodo = m->periodo_total_us + periodo_us;
    if (!isfinite(total) || !isfinite(periodo))
        return ASM_ERROR;
    m->tiempo_total_us = total;
    m->periodo_total_us = periodo;
    if (tiempo_us > m->tiempo_max_us)
        m->tiempo_max_us = tiempo_us;
    m->bloques_tiempo++;
    return ASM_OK;
}

int metricas_calcular(const metricas_t *m, resultado_metricas_t *r)
{
    if (!estado_valido(m) || !r || m->muestras == 0 ||
        (m->bloques_tiempo && m->periodo_total_us == 0.0))
        return ASM_ERROR;
    resultado_metricas_t resultado;
    resultado.mse = (float)(m->suma_err_sal2 / m->muestras);
    resultado.snr_entrada_db = cociente_db(m->suma_ref2, m->suma_err_ent2);
    resultado.snr_salida_db = cociente_db(m->suma_ref2, m->suma_err_sal2);
    // La referencia se cancela. calcular directamente evita restar dos infinitos.
    resultado.delta_snr_db = m->suma_ref2 > 0.0
                                 ? cociente_db(m->suma_err_ent2, m->suma_err_sal2)
                                 : NAN;
    resultado.energia_util_pct = NAN;
    resultado.atenuacion_interferencia_db = NAN;
    resultado.tiempo_medio_us = NAN;
    resultado.tiempo_max_us = NAN;
    resultado.carga_pct = NAN;
    if (m->bloques_bandas)
    {
        // Sin energia util de referencia no existe un porcentaje interpretable.
        if (m->energia_util_ref > 0.0)
            resultado.energia_util_pct = (float)(100.0 * (m->energia_util_sal / m->energia_util_ref));
        resultado.atenuacion_interferencia_db = cociente_db(m->energia_interferencia_ent,
                                                            m->energia_interferencia_sal);
    }
    if (m->bloques_tiempo)
    {
        resultado.tiempo_medio_us = (float)(m->tiempo_total_us / m->bloques_tiempo);
        resultado.tiempo_max_us = (float)m->tiempo_max_us;
        resultado.carga_pct = (float)(100.0 * (m->tiempo_total_us / m->periodo_total_us));
    }
    *r = resultado;
    return ASM_OK;
}

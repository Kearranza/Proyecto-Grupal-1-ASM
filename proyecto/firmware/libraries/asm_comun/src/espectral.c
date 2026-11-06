#include <math.h>
#include <string.h>
#include "espectral.h"
#include "fft.h"

int espectral_iniciar(espectral_t *e, int n)
{
    if (!e || !n_es_valido(n))
        return ASM_ERROR;
    memset(e, 0, sizeof(*e));
    e->n = n;
    e->umbral = 1.0f;
    return ASM_OK;
}

int espectral_procesar_salto(espectral_t *e, const float *nuevas, float *salida,
                             const diagnostico_t *d, float fs)
{
    // Un diagnostico de otro N puede estar atrasado tras un cambio de bloque
    // Rechazarlo antes de consumir muestras permite al llamador resincronizar
    if (!e || !nuevas || !salida || !d || !n_es_valido(e->n) ||
        !isfinite(fs) || fs <= 0.0f || d->n_bloque != e->n ||
        d->tipo > RUIDO_IMPULSIVO || !isfinite(e->umbral) ||
        e->umbral < 0.0f || e->umbral > 1.0f ||
        !isfinite(e->ancho_rechazo_hz) || e->ancho_rechazo_hz < 0.0f ||
        e->ancho_rechazo_hz > fs || !isfinite(d->nivel_db))
        return ASM_ERROR;

    int localizado = d->tipo == RUIDO_TONAL || d->tipo == RUIDO_BANDA;
    if (localizado && (!isfinite(d->frecuencia_hz) || d->frecuencia_hz < 0.0f ||
                       d->frecuencia_hz > fs / 2.0f || !isfinite(d->ancho_hz) ||
                       d->ancho_hz < 0.0f || d->ancho_hz > fs))
        return ASM_ERROR;

    const int n = e->n, salto = n / 2;
    for (int i = 0; i < salto; i++)
        if (!isfinite(nuevas[i]))
            return ASM_ERROR;

    memmove(e->entrada, e->entrada + salto, salto * sizeof(float));
    memcpy(e->entrada + salto, nuevas, salto * sizeof(float));
    for (int i = 0; i < n; i++)
    {
        // Hann periodica: w[i] + w[i+N/2] = 1. No se aplica otra ventana
        // porque altera la ganancia del solapamiento-suma.
        float ventana = 0.5f - 0.5f * cosf(6.2831853071795864769f * i / n);
        e->re[i] = e->entrada[i] * ventana;
        e->im[i] = 0.0f;
    }
    if (fft(e->re, e->im, n) != ASM_OK)
        return ASM_ERROR;

    float potencia_ruido = 0.0f;
    if (d->tipo == RUIDO_BLANCO || d->tipo == RUIDO_IMPULSIVO)
    {
        float total = 0.0f;
        for (int k = 0; k < n; k++)
            total += e->re[k] * e->re[k] + e->im[k] * e->im[k];
        // nivel_db expresa ruido/senal. La fraccion se calcula sin elevar
        // diez a un exponente positivo, para evitar desbordar con niveles altos.
        float relacion = powf(10.0f, -fabsf(d->nivel_db) / 10.0f);
        float fraccion = d->nivel_db >= 0.0f ? 1.0f / (1.0f + relacion)
                                             : relacion / (1.0f + relacion);
        potencia_ruido = (total / n) * fraccion;
    }

    const float bin_hz = fs / n;
    // El ancho configurado es total. Cero selecciona el ancho del diagnostico.
    // El margen de dos bins cubre la ventana principal de Hann.
    float ancho = e->ancho_rechazo_hz > 0.0f ? e->ancho_rechazo_hz : d->ancho_hz;
    float radio = ancho / 2.0f + 2.0f * bin_hz;
    for (int k = 0; k <= salto; k++)
    {
        float ganancia = 1.0f;
        if (localizado)
        {
            float distancia = fabsf(k * bin_hz - d->frecuencia_hz);
            float rechazo = 0.0f;
            if (distancia <= radio)
                rechazo = 1.0f;
            else if (distancia < radio + bin_hz)
                rechazo = 0.5f + 0.5f * cosf(3.14159265358979323846f * (distancia - radio) / bin_hz);
            ganancia = 1.0f - e->umbral * rechazo;
        }
        else if (d->tipo == RUIDO_BLANCO || d->tipo == RUIDO_IMPULSIVO)
        {
            // Sustraccion de potencia con fase conservada, el modelo uniforme
            // es aproximado para impulsos.
            float potencia = e->re[k] * e->re[k] + e->im[k] * e->im[k];
            if (potencia > 0.0f)
                ganancia = sqrtf(fmaxf(0.0f, 1.0f - e->umbral * potencia_ruido / potencia));
        }
        e->re[k] *= ganancia;
        e->im[k] *= ganancia;
        if (k == 0 || k == salto)
            e->im[k] = 0.0f;
        else
        {
            // Escribir el conjugado evita que el redondeo produzca salida compleja.
            e->re[n - k] = e->re[k];
            e->im[n - k] = -e->im[k];
        }
    }
    if (ifft(e->re, e->im, n) != ASM_OK)
        return ASM_ERROR;
    for (int i = 0; i < salto; i++)
    {
        salida[i] = e->acumulado[i] + e->re[i];
        e->acumulado[i] = e->re[i + salto];
    }
    return ASM_OK;
}

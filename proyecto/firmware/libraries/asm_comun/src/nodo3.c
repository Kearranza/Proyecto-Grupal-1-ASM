#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "nodo3.h"
#include "fft.h"

int nodo3_iniciar(nodo3_t *p, int n, float umbral, float ancho)
{
    if (!p || !n_es_valido(n) || !isfinite(umbral) || umbral < 0 || umbral > 1 ||
        !isfinite(ancho) || ancho < 0 || ancho > FS_HZ)
        return ASM_ERROR;
    memset(p, 0, sizeof(*p));
    espectral_iniciar(&p->filtro, n);
    p->filtro.umbral = umbral;
    p->filtro.ancho_rechazo_hz = ancho;
    return ASM_OK;
}

static int energias(nodo3_t *p, const float *x, int salto, const bandas_nodo3_t *b,
                    double *util, double *ruido)
{
    memcpy(p->re, x, salto * sizeof(float));
    memset(p->im, 0, salto * sizeof(float));
    if (fft(p->re, p->im, salto) != ASM_OK)
        return ASM_ERROR;
    if (metricas_energia_banda(p->re, p->im, salto, FS_HZ, b->util_desde, b->util_hasta, util) != ASM_OK)
        return ASM_ERROR;
    return metricas_energia_banda(p->re, p->im, salto, FS_HZ, b->ruido_desde, b->ruido_hasta, ruido);
}

int nodo3_procesar(nodo3_t *p, const float *x, const float *referencia, float *y,
                   const diagnostico_t *d, const bandas_nodo3_t *b)
{
    if (!p || !x || !y || !d || x == y || referencia == y || p->filtro.n < 16 || !n_es_valido(p->filtro.n))
        return ASM_ERROR;
    int salto = p->filtro.n / 2;
    if (b && b->activas && (!isfinite(b->util_desde) || !isfinite(b->util_hasta) || !isfinite(b->ruido_desde) || !isfinite(b->ruido_hasta) || b->util_desde < 0 || b->util_hasta < b->util_desde || b->util_hasta > FS_HZ / 2 || b->ruido_desde < 0 || b->ruido_hasta < b->ruido_desde || b->ruido_hasta > FS_HZ / 2))
        return ASM_ERROR;
    if (referencia)
        for (int i = 0; i < salto; i++)
            if (!isfinite(referencia[i]))
                return ASM_ERROR;
    if (espectral_procesar_salto(&p->filtro, x, y, d, FS_HZ) != ASM_OK)
        return ASM_ERROR;
    if (p->referencia_anterior_valida)
    {
        // La salida de este salto corresponde a la entrada del salto anterior.
        if (metricas_acumular(&p->metricas, p->referencia_anterior, p->entrada_anterior, y, salto) != ASM_OK)
            return ASM_ERROR;
        if (b && b->activas)
        {
            double ur, us, ie, is, ignorada;
            if (energias(p, p->referencia_anterior, salto, b, &ur, &ignorada) != ASM_OK ||
                energias(p, p->entrada_anterior, salto, b, &ignorada, &ie) != ASM_OK ||
                energias(p, y, salto, b, &us, &is) != ASM_OK ||
                metricas_acumular_bandas(&p->metricas, ur, us, ie, is) != ASM_OK)
                return ASM_ERROR;
        }
    }
    // x e y deben ser arreglos distintos. x sigue siendo necesario al guardar.
    memcpy(p->entrada_anterior, x, salto * sizeof(float));
    if (referencia)
        memcpy(p->referencia_anterior, referencia, salto * sizeof(float));
    p->referencia_anterior_valida = referencia != NULL;
    return ASM_OK;
}

void nodo3_generar_demo(uint32_t primera, int cantidad, float *x, float *referencia)
{
    if (!x || !referencia || cantidad < 0)
        return;
    for (int i = 0; i < cantidad; i++)
    {
        // Reducir el indice conserva la precision despues de muchas pruebas
        uint32_t indice = (primera + (uint32_t)i) % FS_HZ;
        referencia[i] = 0.3f * sinf(6.2831853071795864769f * 500.0f * indice / FS_HZ);
        x[i] = referencia[i] + 0.3f * sinf(6.2831853071795864769f * 3500.0f * indice / FS_HZ);
    }
}

int nodo3_leer_comando(const char *linea, comando_nodo3_t *comando)
{
    if (!linea || !comando)
        return ASM_ERROR;
    char orden[24], argumento[32], extra[2];
    int campos = sscanf(linea, "%23s %31s %1s", orden, argumento, extra);
    comando_nodo3_t c = {N3_ESTADO, 0};
    if (campos == 1)
    {
        if (!strcmp(orden, "ESTADO"))
            c.orden = N3_ESTADO;
        else if (!strcmp(orden, "AYUDA"))
            c.orden = N3_AYUDA;
        else if (!strcmp(orden, "REINICIAR"))
            c.orden = N3_REINICIAR;
        else if (!strcmp(orden, "DEMO"))
            c.orden = N3_DEMO;
        else if (!strcmp(orden, "VIVO"))
            c.orden = N3_VIVO;
        else
            return ASM_ERROR;
    }
    else if (campos == 2)
    {
        if (!strcmp(orden, "METODO"))
        {
            if (!strcmp(argumento, "ESPECTRAL"))
                c.orden = N3_ESPECTRAL;
            else if (!strcmp(argumento, "PASO"))
                c.orden = N3_PASO;
            else
                return ASM_ERROR;
        }
        else
        {
            char *fin;
            errno = 0;
            c.valor = strtof(argumento, &fin);
            if (errno || *fin || !isfinite(c.valor))
                return ASM_ERROR;
            if (!strcmp(orden, "N"))
            {
                if (c.valor != 256 && c.valor != 512 && c.valor != 1024 && c.valor != 2048)
                    return ASM_ERROR;
                c.orden = N3_N;
            }
            else if (!strcmp(orden, "UMBRAL"))
            {
                if (c.valor < 0 || c.valor > 1)
                    return ASM_ERROR;
                c.orden = N3_UMBRAL;
            }
            else if (!strcmp(orden, "ANCHO"))
            {
                if (c.valor < 0 || c.valor > FS_HZ)
                    return ASM_ERROR;
                c.orden = N3_ANCHO;
            }
            else
                return ASM_ERROR;
        }
    }
    else
        return ASM_ERROR;
    *comando = c;
    return ASM_OK;
}

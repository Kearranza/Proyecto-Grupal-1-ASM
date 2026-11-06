// Integracion independiente de Arduino para validar el flujo completo en la PC.
#pragma once
#include "espectral.h"
#include "metricas.h"
#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        espectral_t filtro;
        metricas_t metricas;
        float entrada_anterior[SALTO_MAX], referencia_anterior[SALTO_MAX];
        float re[SALTO_MAX], im[SALTO_MAX];
        int referencia_anterior_valida;
    } nodo3_t;

    typedef struct
    {
        int activas;
        float util_desde, util_hasta, ruido_desde, ruido_hasta;
    } bandas_nodo3_t;

    int nodo3_iniciar(nodo3_t *p, int n, float umbral, float ancho);
    // referencia puede ser NULL: procesa audio, pero no inventa MSE, SNR ni energia.
    // Una referencia corresponde al salto actual, antes del filtro.
    // y no debe sobreponerse sobre x, referencia ni el estado p, N minimo 16 para FFT de bandas.
    int nodo3_procesar(nodo3_t *p, const float *x, const float *referencia, float *y,
                       const diagnostico_t *d, const bandas_nodo3_t *bandas);
    // Tono util de 500 Hz y ruido de 3500 Hz, amplitud 0.3 cada uno. solo demo.
    void nodo3_generar_demo(uint32_t primera, int cantidad, float *x, float *referencia);

    typedef enum
    {
        N3_ESTADO,
        N3_AYUDA,
        N3_REINICIAR,
        N3_DEMO,
        N3_VIVO,
        N3_N,
        N3_UMBRAL,
        N3_ANCHO,
        N3_ESPECTRAL,
        N3_PASO
    } orden_nodo3_t;
    typedef struct
    {
        orden_nodo3_t orden;
        float valor;
    } comando_nodo3_t;
    // Acepta una linea completa, rechaza argumentos extra y numeros no finitos.
    int nodo3_leer_comando(const char *linea, comando_nodo3_t *comando);
#ifdef __cplusplus
}
#endif

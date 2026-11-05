#include "prueba.h"
#include "nodo3.h"
#include <string.h>

static nodo3_t p;
static float x[SALTO_MAX], s[SALTO_MAX], y[SALTO_MAX];

static void probar_flujo(int n, float umbral)
{
    diagnostico_t d = {RUIDO_TONAL, 0, (uint16_t)n, 3500, 0, 0};
    bandas_nodo3_t b = {1, 437.5f, 562.5f, 3437.5f, 3562.5f};
    int bien = nodo3_iniciar(&p, n, umbral, 0) == ASM_OK;
    const int salto = n / 2;
    for (int i = 0; i < 32 && bien; i++)
    {
        nodo3_generar_demo((uint32_t)i * salto, salto, x, s);
        bien &= nodo3_procesar(&p, x, s, y, &d, &b) == ASM_OK;
        if (i == 0)
            bien &= p.metricas.muestras == 0;
    }
    resultado_metricas_t r;
    bien &= metricas_calcular(&p.metricas, &r) == ASM_OK;
    bien &= p.metricas.muestras == 31 * salto;
    if (bien && isfinite(r.mse) && isfinite(r.delta_snr_db) &&
        (umbral ? r.delta_snr_db > 20 && r.atenuacion_interferencia_db > 20 && fabsf(r.energia_util_pct - 100) < 2
                : fabsf(r.delta_snr_db) < 0.01f && fabsf(r.atenuacion_interferencia_db) < 0.01f))
    {
        OK("demo -> filtro -> alineacion -> metricas y bandas");
        printf("              N=%d umbral=%.1f mejora=%.2f dB energia=%.2f %%\n", n, umbral, r.delta_snr_db, r.energia_util_pct);
    }
    else
        FALLA("flujo integrado", "N=%d umbral=%.1f", n, umbral);
}

static void probar_sin_referencia(void)
{
    diagnostico_t d = {RUIDO_NINGUNO, 0, 512, 0, 0, 0};
    nodo3_iniciar(&p, 512, 1, 0);
    int bien = 1;
    for (int i = 0; i < 4; i++)
    {
        nodo3_generar_demo(i * 256, 256, x, s);
        bien &= nodo3_procesar(&p, x, NULL, y, &d, NULL) == ASM_OK;
    }
    resultado_metricas_t r;
    bien &= p.metricas.muestras == 0 && metricas_calcular(&p.metricas, &r) == ASM_ERROR;
    if (bien)
        OK("audio vivo no inventa una referencia limpia");
    else
        FALLA("referencia ausente", "se calcularon metricas o se rechazo audio");
    nodo3_iniciar(&p, 256, 0.5f, 200);
    if (p.metricas.muestras == 0 && !p.referencia_anterior_valida && p.filtro.n == 256 && p.filtro.umbral == 0.5f)
        OK("reinicio limpia historial y aplica parametros");
    else
        FALLA("reinicio", "estado residual");
}

static void probar_comandos(void)
{
    const char *validos[] = {"ESTADO", "AYUDA", "DEMO", "VIVO", "REINICIAR", "N 512", "UMBRAL 0.3", "ANCHO 250", "METODO PASO", "METODO ESPECTRAL", " N 2048 \r\n"};
    const char *invalidos[] = {"", "N", "N 12", "N 256 basura", "N nan", "N inf", "UMBRAL -1", "UMBRAL 1.1", "ANCHO 16001", "ANCHO 4hz", "METODO IIR", "ESTADO 1", "DESCONOCIDO", "UMBRAL 0.5 1"};
    comando_nodo3_t c;
    int bien = 1;
    for (size_t i = 0; i < sizeof validos / sizeof validos[0]; i++)
        bien &= nodo3_leer_comando(validos[i], &c) == ASM_OK;
    for (size_t i = 0; i < sizeof invalidos / sizeof invalidos[0]; i++)
        bien &= nodo3_leer_comando(invalidos[i], &c) == ASM_ERROR;
    nodo3_leer_comando("UMBRAL 0.3", &c);
    bien &= c.orden == N3_UMBRAL && fabsf(c.valor - 0.3f) < 1e-6f;
    if (bien)
        OK("comandos USB: sintaxis, limites y argumentos extra");
    else
        FALLA("comandos", "se acepto o rechazo una orden incorrecta");
}

int main(void)
{
    printf("\n== nodo3.c ==\n");
    for (int n = 256; n <= N_MAX; n *= 2)
    {
        probar_flujo(n, 1);
        probar_flujo(n, 0);
    }
    probar_sin_referencia();
    probar_comandos();
    return resumen("nodo3.c");
}

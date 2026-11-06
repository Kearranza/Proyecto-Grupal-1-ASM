// mismos valores que src/metricas.py.
#include "prueba.h"
#include <string.h>
#include <limits.h>

#define TOL_REL 1e-3

static void probar_referencia(void)
{
    printf("\n== metricas.c ==\n");
    const char *nombre = "MSE, SNR y delta SNR igual que src/metricas.py";
    FILE *f = abrir_referencia("datos/metricas.txt", nombre);
    if (!f)
        return;
    int n;
    static float s[8192], x[8192], y[8192];
    float esperado[4];
    int valido = fscanf(f, "%d", &n) == 1 && n > 0 && n <= 8192;
    for (int i = 0; valido && i < n; i++)
        valido = fscanf(f, "%f %f %f", &s[i], &x[i], &y[i]) == 3;
    for (int i = 0; valido && i < 4; i++)
        valido = fscanf(f, "%f", &esperado[i]) == 1;
    fclose(f);
    if (!valido)
    {
        FALLA(nombre, "archivo invalido");
        return;
    }

    metricas_t m;
    metricas_reiniciar(&m);
    // Se acumula por saltos de 512, como lo hara el nodo 3
    int r = ASM_OK;
    for (int i = 0; i < n && r == ASM_OK; i += 512)
        r = metricas_acumular(&m, s + i, x + i, y + i, n - i < 512 ? n - i : 512);
    if (r != ASM_OK)
    {
        FALLA(nombre, "acumulacion devolvio %d", r);
        return;
    }
    if (es_pendiente(r, nombre, "Gretchell"))
        return;
    resultado_metricas_t res;
    r = metricas_calcular(&m, &res);
    if (es_pendiente(r, nombre, "Gretchell"))
        return;

    if (r != ASM_OK)
    {
        FALLA(nombre, "calculo devolvio %d", r);
        return;
    }
    float obtenido[4] = {res.mse, res.snr_entrada_db, res.snr_salida_db, res.delta_snr_db};
    const char *nombres[4] = {"MSE", "SNR entrada", "SNR salida", "delta SNR"};
    int bien = 1;
    for (int i = 0; i < 4; i++)
    {
        if (!isfinite(obtenido[i]) || fabsf(obtenido[i] - esperado[i]) > TOL_REL * fmaxf(1.0f, fabsf(esperado[i])))
        {
            FALLA(nombre, "%s = %g, se esperaba %g", nombres[i], obtenido[i], esperado[i]);
            bien = 0;
        }
    }
    if (bien)
        OK(nombre);
    return;
}

static int cerca(double a, double b)
{
    return isfinite(a) && fabs(a - b) <= 1e-5 * fmax(1.0, fabs(b));
}

static void probar_formulas(void)
{
    float s[] = {1, -1, 1, -1}, x[] = {2, 0, 2, 0}, y[] = {1.5f, -0.5f, 1.5f, -0.5f};
    metricas_t m, parcial;
    resultado_metricas_t r, rp;
    metricas_reiniciar(&m);
    metricas_reiniciar(&parcial);
    int bien = metricas_acumular(&m, s, x, y, 4) == ASM_OK &&
               metricas_acumular(&parcial, s, x, y, 1) == ASM_OK &&
               metricas_acumular(&parcial, s + 1, x + 1, y + 1, 3) == ASM_OK &&
               metricas_calcular(&m, &r) == ASM_OK && metricas_calcular(&parcial, &rp) == ASM_OK;
    if (bien && cerca(r.mse, 0.25) && cerca(r.snr_entrada_db, 0) &&
        cerca(r.snr_salida_db, 10 * log10(4)) && cerca(r.delta_snr_db, 10 * log10(4)) &&
        cerca(rp.mse, r.mse) && cerca(rp.delta_snr_db, r.delta_snr_db) && parcial.muestras == 4)
        OK("formulas conocidas y saltos de distintos largos");
    else
        FALLA("formulas", "valores incorrectos");
    if (bien && isnan(r.energia_util_pct) && isnan(r.atenuacion_interferencia_db) &&
        isnan(r.tiempo_medio_us) && isnan(r.tiempo_max_us) && isnan(r.carga_pct))
        OK("datos opcionales ausentes se indican con NaN");
    else
        FALLA("opcionales", "no se deben inventar mediciones");
}

static void probar_ceros(void)
{
    float uno = 1, cero = 0, dos = 2;
    metricas_t m;
    resultado_metricas_t r;
    metricas_reiniciar(&m);
    int bien = metricas_acumular(&m, &uno, &dos, &uno, 1) == ASM_OK && metricas_calcular(&m, &r) == ASM_OK;
    if (bien && r.mse == 0 && r.snr_salida_db == INFINITY && r.delta_snr_db == INFINITY)
        OK("reconstruccion exacta da SNR infinita");
    else
        FALLA("error cero", "resultado incorrecto");
    metricas_reiniciar(&m);
    bien = metricas_acumular(&m, &uno, &uno, &uno, 1) == ASM_OK && metricas_calcular(&m, &r) == ASM_OK;
    if (bien && isnan(r.delta_snr_db))
        OK("dos errores nulos: mejora indefinida");
    else
        FALLA("dos errores nulos", "delta debe ser NaN");
    metricas_reiniciar(&m);
    bien = metricas_acumular(&m, &cero, &uno, &cero, 1) == ASM_OK && metricas_calcular(&m, &r) == ASM_OK;
    if (bien && r.snr_entrada_db == -INFINITY && isnan(r.snr_salida_db) && isnan(r.delta_snr_db))
        OK("referencia silenciosa: SNR y mejora sin referencia util");
    else
        FALLA("silencio", "resultado incorrecto");
}

static void probar_bandas_tiempos(void)
{
    metricas_t m;
    resultado_metricas_t r;
    float uno = 1;
    metricas_reiniciar(&m);
    int bien = metricas_acumular(&m, &uno, &uno, &uno, 1) == ASM_OK &&
               metricas_acumular_bandas(&m, 1, 0.25, 1, 0.01) == ASM_OK &&
               metricas_acumular_bandas(&m, 3, 1.75, 3, 0.03) == ASM_OK &&
               metricas_acumular_tiempo(&m, 100, 1000) == ASM_OK &&
               metricas_acumular_tiempo(&m, 300, 3000) == ASM_OK && metricas_calcular(&m, &r) == ASM_OK;
    if (bien && cerca(r.energia_util_pct, 50) && cerca(r.atenuacion_interferencia_db, 20))
        OK("bandas: cociente de energias acumuladas, no promedio de porcentajes");
    else
        FALLA("bandas", "energia o atenuacion incorrectas");
    if (bien && cerca(r.tiempo_medio_us, 200) && cerca(r.tiempo_max_us, 300) && cerca(r.carga_pct, 10))
        OK("tiempo medio, maximo y carga ponderada por periodo");
    else
        FALLA("tiempos", "resultado incorrecto");
    metricas_reiniciar(&m);
    bien = metricas_acumular(&m, &uno, &uno, &uno, 1) == ASM_OK &&
           metricas_acumular_bandas(&m, 1, 2, 1, 0) == ASM_OK &&
           metricas_acumular_tiempo(&m, 1500, 1000) == ASM_OK && metricas_calcular(&m, &r) == ASM_OK;
    if (bien && cerca(r.energia_util_pct, 200) && r.atenuacion_interferencia_db == INFINITY && cerca(r.carga_pct, 150))
        OK("sin recortar ganancia, sobrecarga ni rechazo completo");
    else
        FALLA("limites", "se perdio informacion");
    metricas_reiniciar(&m);
    bien = metricas_acumular(&m, &uno, &uno, &uno, 1) == ASM_OK &&
           metricas_acumular_bandas(&m, 0, 0, 0, 0) == ASM_OK && metricas_calcular(&m, &r) == ASM_OK;
    if (bien && isnan(r.energia_util_pct) && isnan(r.atenuacion_interferencia_db))
        OK("bandas vacias indefinidas");
    else
        FALLA("bandas vacias", "deben ser NaN");
}

static void probar_parseval(void)
{
    float re[256], im[256] = {0};
    for (int i = 0; i < 256; i++)
        re[i] = 0.25f + (i % 2 ? -0.125f : 0.125f) + 0.5f * sinf(2.0f * (float)M_PI * 500 * i / FS_HZ);
    double total, dc, nyquist, tono, vacia;
    int bien = fft(re, im, 256) == ASM_OK &&
               metricas_energia_banda(re, im, 256, FS_HZ, 0, FS_HZ / 2, &total) == ASM_OK &&
               metricas_energia_banda(re, im, 256, FS_HZ, 0, 0, &dc) == ASM_OK &&
               metricas_energia_banda(re, im, 256, FS_HZ, FS_HZ / 2, FS_HZ / 2, &nyquist) == ASM_OK &&
               metricas_energia_banda(re, im, 256, FS_HZ, 500, 500, &tono) == ASM_OK &&
               metricas_energia_banda(re, im, 256, FS_HZ, 501, 502, &vacia) == ASM_OK;
    if (bien && cerca(total, 52) && cerca(dc, 16) && cerca(nyquist, 4) && cerca(tono, 32) && vacia == 0)
        OK("Parseval, DC, Nyquist, banda de un bin y banda sin bins");
    else
        FALLA("Parseval", "normalizacion o duplicacion incorrectas");
}

static void probar_errores(void)
{
    metricas_t m, copia;
    resultado_metricas_t r, anterior;
    float s[] = {1, 2}, x[] = {1, NAN}, y[] = {1, 2};
    metricas_reiniciar(NULL);
    metricas_reiniciar(&m);
    memset(&r, 0, sizeof r);
    anterior = r;
    int bien = metricas_calcular(&m, &r) == ASM_ERROR && memcmp(&r, &anterior, sizeof r) == 0;
    copia = m;
    bien &= metricas_acumular(&m, s, x, y, 2) == ASM_ERROR;
    bien &= metricas_acumular(&m, s, s, y, 0) == ASM_ERROR;
    bien &= metricas_acumular(&m, NULL, s, y, 2) == ASM_ERROR;
    bien &= metricas_acumular(NULL, s, s, y, 2) == ASM_ERROR;
    bien &= metricas_acumular_bandas(&m, 1, -1, 1, 1) == ASM_ERROR;
    bien &= metricas_acumular_bandas(&m, 1, 1, INFINITY, 1) == ASM_ERROR;
    bien &= metricas_acumular_tiempo(&m, 10, 0) == ASM_ERROR;
    bien &= memcmp(&m, &copia, sizeof m) == 0;
    m.muestras = LONG_MAX;
    bien &= metricas_acumular(&m, s, s, y, 2) == ASM_ERROR;
    metricas_reiniciar(&m);
    bien &= metricas_calcular(NULL, &r) == ASM_ERROR && metricas_calcular(&m, NULL) == ASM_ERROR;
    float re[8] = {0}, im[8] = {0};
    double energia = -123;
    bien &= metricas_energia_banda(re, im, 8, FS_HZ, 200, 100, &energia) == ASM_ERROR && energia == -123;
    bien &= metricas_energia_banda(re, im, 8, 0, 0, 0, &energia) == ASM_ERROR;
    bien &= metricas_energia_banda(NULL, im, 8, FS_HZ, 0, 100, &energia) == ASM_ERROR;
    bien &= metricas_energia_banda(re, im, 8, FS_HZ, 0, 100, NULL) == ASM_ERROR;
    re[1] = NAN;
    bien &= metricas_energia_banda(re, im, 8, FS_HZ, 0, 100, &energia) == ASM_ERROR;
    if (bien)
        OK("errores atomicos, punteros, no finitos y contador agotado");
    else
        FALLA("errores", "retorno o mutacion incorrectos");
}

int main(void)
{
    probar_referencia();
    probar_formulas();
    probar_ceros();
    probar_bandas_tiempos();
    probar_parseval();
    probar_errores();
    return resumen("metricas.c");
}

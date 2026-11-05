// Contrato de espectral.c con propiedades en vez de muestras exactas:
//  1. Sin ruido que quitar, la salida es la entrada retrasada (reconstruccion perfecta).
//  2. Con interferencia tonal, la atenua sin dañar el tono util.
#include "prueba.h"
#include <string.h>

#define LARGO 16384
#define ERROR_MAX_IDENTIDAD 1e-3f
#define ATEN_MIN_DB 20.0f
#define PERDIDA_UTIL_MAX_DB 1.0f

static float entrada[LARGO], salida[LARGO];
static espectral_t e;

static int procesar(int n, const diagnostico_t *d)
{
    int salto = n / 2, r = espectral_iniciar(&e, n);
    for (int i = 0; r == ASM_OK && i + salto <= LARGO; i += salto)
        r = espectral_procesar_salto(&e, entrada + i, salida + i, d, FS_HZ);
    return r;
}

// Amplitud de un seno de frecuencia f en x[desde..desde+largo) por correlacion
static float amplitud(const float *x, int desde, int largo, float f)
{
    double c = 0.0, s = 0.0;
    for (int i = desde; i < desde + largo; i++)
    {
        c += x[i] * cos(2.0 * M_PI * f * i / FS_HZ);
        s += x[i] * sin(2.0 * M_PI * f * i / FS_HZ);
    }
    return (float)(2.0 * hypot(c, s) / largo);
}

static void probar_identidad(int n)
{
    char nombre[64];
    snprintf(nombre, sizeof nombre, "reconstruccion perfecta sin ruido, n = %d", n);
    // Mezclar DC, Nyquist y datos no periodicos detecta errores que un seno oculta.
    for (int i = 0; i < LARGO; i++)
        entrada[i] = 0.1f + (i % 2 ? -0.1f : 0.1f) + 0.5f * ((float)rand() / RAND_MAX - 0.5f);
    diagnostico_t d = {RUIDO_NINGUNO, 0, (uint16_t)n, 0.0f, 0.0f, 0.0f};
    int r = procesar(n, &d);
    if (es_pendiente(r, nombre, "Gretchell"))
        return;
    if (r != ASM_OK)
    {
        FALLA(nombre, "devolvio %d", r);
        return;
    }
    float error = 0.0f;
    const int salto = n / 2;
    for (int i = 0; i < LARGO; i++)
    {
        float esperado = i < salto ? 0.0f : entrada[i - salto];
        error = fmaxf(error, fabsf(salida[i] - esperado));
    }
    // El primer salto de ceros recupera el final, el segundo deja la cola vacia.
    float ceros[SALTO_MAX] = {0}, cola[SALTO_MAX];
    if (espectral_procesar_salto(&e, ceros, cola, &d, FS_HZ) != ASM_OK)
    {
        FALLA(nombre, "fallo al vaciar");
        return;
    }
    for (int i = 0; i < salto; i++)
        error = fmaxf(error, fabsf(cola[i] - entrada[LARGO - salto + i]));
    if (espectral_procesar_salto(&e, ceros, cola, &d, FS_HZ) != ASM_OK)
    {
        FALLA(nombre, "fallo al vaciar la cola");
        return;
    }
    for (int i = 0; i < salto; i++)
        error = fmaxf(error, fabsf(cola[i]));
    if (error <= ERROR_MAX_IDENTIDAD)
        OK(nombre);
    else
        FALLA(nombre, "error %.2e, retraso esperado %d", error, salto);
}

static void probar_tonal(void)
{
    const char *nombre = "atenua 3500 Hz y conserva 440 Hz";
    for (int i = 0; i < LARGO; i++)
        entrada[i] = 0.4f * sinf(2.0f * (float)M_PI * 440.0f * i / FS_HZ) + 0.4f * sinf(2.0f * (float)M_PI * 3500.0f * i / FS_HZ);
    diagnostico_t d = {RUIDO_TONAL, 0, N_POR_DEFECTO, 3500.0f, 0.0f, 0.0f};
    int r = procesar(N_POR_DEFECTO, &d);
    if (es_pendiente(r, nombre, "Gretchell"))
        return;
    if (r != ASM_OK)
    {
        FALLA(nombre, "devolvio %d", r);
        return;
    }
    int desde = 4 * N_POR_DEFECTO, largo = LARGO - desde - N_POR_DEFECTO;
    float aten = 20.0f * log10f(amplitud(entrada, desde, largo, 3500.0f) / (amplitud(salida, desde, largo, 3500.0f) + 1e-9f));
    float perdida = 20.0f * log10f(amplitud(entrada, desde, largo, 440.0f) / (amplitud(salida, desde, largo, 440.0f) + 1e-9f));
    if (aten >= ATEN_MIN_DB && fabsf(perdida) <= PERDIDA_UTIL_MAX_DB)
        OK(nombre);
    else
        FALLA(nombre, "atenuacion %.1f dB (min %.0f), cambio en 440 Hz %.2f dB (max %.1f)",
              aten, ATEN_MIN_DB, perdida, PERDIDA_UTIL_MAX_DB);
}

static void probar_adaptacion(void)
{
    const int n = 1024, salto = n / 2;
    diagnostico_t d = {RUIDO_TONAL, 0, n, 2317.0f, 0.0f, 0.0f};
    espectral_iniciar(&e, n);
    for (int i = 0; i < LARGO; i++)
    {
        float f = i < LARGO / 2 ? 2317.0f : 3513.0f;
        entrada[i] = 0.4f * sinf(2.0f * (float)M_PI * f * i / FS_HZ) + 0.3f * sinf(2.0f * (float)M_PI * 500.0f * i / FS_HZ);
    }
    for (int i = 0; i < LARGO; i += salto)
    {
        if (i == LARGO / 2)
            d.frecuencia_hz = 3513.0f;
        if (espectral_procesar_salto(&e, entrada + i, salida + i, &d, FS_HZ) != ASM_OK)
        {
            FALLA("adaptacion tonal", "salto rechazado");
            return;
        }
    }
    for (int tramo = 0; tramo < 2; tramo++)
    {
        int desde = tramo * LARGO / 2 + 2 * n, largo = LARGO / 2 - 3 * n;
        float f = tramo ? 3513.0f : 2317.0f;
        float aten = 20.0f * log10f(0.4f / (amplitud(salida, desde, largo, f) + 1e-9f));
        float util = amplitud(salida, desde, largo, 500.0f);
        if (aten < ATEN_MIN_DB || fabsf(20.0f * log10f(util / 0.3f)) > PERDIDA_UTIL_MAX_DB)
            FALLA("adaptacion tonal fuera de bin", "f=%.0f, atenuacion=%.2f, util=%.3f", f, aten, util);
        else
            OK("adaptacion tonal fuera de bin");
    }
}

static void probar_banda(void)
{
    diagnostico_t d = {RUIDO_BANDA, 0, 1024, 3000.0f, 600.0f, 0.0f};
    for (int i = 0; i < LARGO; i++)
        entrada[i] = 0.2f * sinf(2.0f * (float)M_PI * 2800.0f * i / FS_HZ) + 0.2f * sinf(2.0f * (float)M_PI * 3200.0f * i / FS_HZ) + 0.3f * sinf(2.0f * (float)M_PI * 500.0f * i / FS_HZ);
    if (procesar(1024, &d) != ASM_OK)
    {
        FALLA("banda", "salto rechazado");
        return;
    }
    float a = amplitud(salida, 4096, 8192, 2800.0f);
    float b = amplitud(salida, 4096, 8192, 3200.0f);
    float util = amplitud(salida, 4096, 8192, 500.0f);
    if (a < 0.02f && b < 0.02f && fabsf(util - 0.3f) < 0.01f)
        OK("rechazo de banda y conservacion fuera de ella");
    else
        FALLA("banda", "amplitudes %.4f, %.4f, util %.4f", a, b, util);
}

static void probar_ancha(void)
{
    // Ruido uniforme con potencia 0.16/3; seno de potencia 0.25/2.
    diagnostico_t d = {RUIDO_BLANCO, 0, 1024, 0, 0, 10.0f * log10f((0.16f / 3.0f) / 0.125f)};
    for (int i = 0; i < LARGO; i++)
        entrada[i] = 0.5f * sinf(2.0f * (float)M_PI * 500.0f * i / FS_HZ) + 0.4f * (2.0f * (float)rand() / RAND_MAX - 1.0f);
    if (procesar(1024, &d) != ASM_OK)
    {
        FALLA("banda ancha", "salto rechazado");
        return;
    }
    double antes = 0.0, despues = 0.0;
    for (int i = 2048; i < LARGO; i++)
    {
        float limpia = 0.5f * sinf(2.0f * (float)M_PI * 500.0f * (i - 512) / FS_HZ);
        antes += pow(entrada[i - 512] - limpia, 2);
        despues += pow(salida[i] - limpia, 2);
    }
    double mejora = 10.0 * log10(antes / despues);
    if (isfinite(mejora) && mejora > 2.0)
        OK("banda ancha: mejora SNR mayor a 2 dB en seno sintetico");
    else
        FALLA("banda ancha", "mejora %.2f dB", mejora);
    printf("              mejora SNR = %.2f dB\n", mejora);
}

static void probar_parametros(void)
{
    const int n = 256, salto = n / 2;
    diagnostico_t d = {RUIDO_BANDA, 0, n, 3000.0f, 1000.0f, 0};
    float x[SALTO_MAX], anterior[SALTO_MAX] = {0};
    espectral_iniciar(&e, n);
    e.umbral = 0.0f;
    float error = 0.0f;
    for (int bloque = 0; bloque < 8; bloque++)
    {
        for (int i = 0; i < salto; i++)
            x[i] = (float)rand() / RAND_MAX - 0.5f;
        float copia[SALTO_MAX];
        memcpy(copia, x, salto * sizeof(float));
        if (espectral_procesar_salto(&e, x, x, &d, FS_HZ) != ASM_OK)
        {
            FALLA("umbral cero en sitio", "salto rechazado");
            return;
        }
        for (int i = 0; i < salto; i++)
            error = fmaxf(error, fabsf(x[i] - anterior[i]));
        memcpy(anterior, copia, salto * sizeof(float));
    }
    if (error < ERROR_MAX_IDENTIDAD)
        OK("umbral cero conserva y permite entrada/salida en sitio");
    else
        FALLA("umbral cero", "error %.2e", error);

    // El ancho configurado sustituye al del diagnostico entre saltos.
    float amplitudes[2];
    for (int caso = 0; caso < 2; caso++)
    {
        espectral_iniciar(&e, 1024);
        e.ancho_rechazo_hz = caso ? 1200.0f : 100.0f;
        d.n_bloque = 1024;
        for (int i = 0; i < LARGO; i++)
            entrada[i] = 0.5f * sinf(2.0f * (float)M_PI * 3500.0f * i / FS_HZ);
        for (int i = 0; i < LARGO; i += 512)
            if (espectral_procesar_salto(&e, entrada + i, salida + i, &d, FS_HZ) != ASM_OK)
            {
                FALLA("ancho ajustable", "salto rechazado");
                return;
            }
        amplitudes[caso] = amplitud(salida, 4096, 8192, 3500.0f);
    }
    if (amplitudes[0] > 0.49f && amplitudes[1] < 0.005f)
        OK("ancho de rechazo configurable");
    else
        FALLA("ancho", "amplitudes %.3f, %.3f", amplitudes[0], amplitudes[1]);
}

static void probar_intensidad_e_impulso(void)
{
    diagnostico_t d = {RUIDO_TONAL, 0, 256, 3000.0f, 0.0f, 0.0f};
    espectral_iniciar(&e, 256);
    for (int tramo = 0; tramo < 3; tramo++)
    {
        e.umbral = tramo * 0.5f;
        for (int i = 0; i < LARGO; i++)
            entrada[i] = 0.5f * sinf(2.0f * (float)M_PI * 3000.0f * i / FS_HZ);
        for (int i = 0; i < LARGO; i += 128)
            if (espectral_procesar_salto(&e, entrada + i, salida + i, &d, FS_HZ) != ASM_OK)
            {
                FALLA("intensidad", "salto rechazado");
                return;
            }
        float a = amplitud(salida, 4096, 8192, 3000.0f);
        if (fabsf(a - 0.5f * (1.0f - e.umbral)) < 0.001f)
            OK("cambio de intensidad sin reiniciar");
        else
            FALLA("intensidad", "umbral %.1f, amplitud %.4f", e.umbral, a);
    }
    d.tipo = RUIDO_IMPULSIVO;
    memset(entrada, 0, sizeof entrada);
    entrada[1023] = 0.8f;
    entrada[4096] = -0.8f;
    int bien = procesar(256, &d) == ASM_OK;
    double energia = 0.0;
    for (int i = 0; i < LARGO; i++)
    {
        bien &= isfinite(salida[i]);
        energia += salida[i] * salida[i];
    }
    for (int i = 0; i < 256; i++)
        bien &= fabsf(e.im[i]) < 1e-5f;
    if (bien && energia <= 1.28 && energia > 0)
        OK("impulsos: salida real y finita sin crecimiento de energia");
    else
        FALLA("impulsos", "energia %.5f o salida no real/finita", energia);
}

static void probar_errores_y_reinicio(void)
{
    static espectral_t copia;
    float x[SALTO_MAX] = {0}, y[SALTO_MAX] = {0};
    diagnostico_t d = {RUIDO_NINGUNO, 0, 256, 0, 0, 0};
    espectral_iniciar(&e, 256);
    copia = e;
    int bien = espectral_iniciar(NULL, 256) == ASM_ERROR &&
               espectral_iniciar(&e, 12) == ASM_ERROR &&
               espectral_procesar_salto(NULL, x, y, &d, FS_HZ) == ASM_ERROR &&
               espectral_procesar_salto(&e, NULL, y, &d, FS_HZ) == ASM_ERROR &&
               espectral_procesar_salto(&e, x, NULL, &d, FS_HZ) == ASM_ERROR &&
               espectral_procesar_salto(&e, x, y, NULL, FS_HZ) == ASM_ERROR &&
               espectral_procesar_salto(&e, x, y, &d, NAN) == ASM_ERROR &&
               espectral_procesar_salto(&e, x, y, &d, 0) == ASM_ERROR;
    d.n_bloque = 512;
    bien &= espectral_procesar_salto(&e, x, y, &d, FS_HZ) == ASM_ERROR;
    d.n_bloque = 256;
    x[0] = NAN;
    bien &= espectral_procesar_salto(&e, x, y, &d, FS_HZ) == ASM_ERROR;
    x[0] = 0;
    bien &= memcmp(&e, &copia, sizeof e) == 0;
    e.umbral = -1;
    bien &= espectral_procesar_salto(&e, x, y, &d, FS_HZ) == ASM_ERROR;
    e.umbral = 1;
    d.tipo = RUIDO_TONAL;
    d.frecuencia_hz = FS_HZ;
    bien &= espectral_procesar_salto(&e, x, y, &d, FS_HZ) == ASM_ERROR;
    if (bien)
        OK("argumentos invalidos sin consumir muestras");
    else
        FALLA("validacion", "error de retorno o estado alterado");

    e.entrada[10] = 0.7f;
    e.acumulado[10] = 0.3f;
    espectral_iniciar(&e, 512);
    d.n_bloque = 512;
    d.tipo = RUIDO_IMPULSIVO;
    bien = espectral_procesar_salto(&e, x, y, &d, FS_HZ) == ASM_OK;
    for (int i = 0; i < 256; i++)
        bien &= isfinite(y[i]) && fabsf(y[i]) < 1e-6f;
    if (bien)
        OK("reinicio de N y silencio sin divisiones por cero");
    else
        FALLA("reinicio", "quedan muestras o valores no finitos");
}

int main(void)
{
    printf("\n== espectral.c ==\n");
    srand(7);
    for (int n = 8; n <= N_MAX; n *= 2)
        probar_identidad(n);
    probar_tonal();
    probar_adaptacion();
    probar_banda();
    probar_ancha();
    probar_parametros();
    probar_errores_y_reinicio();
    probar_intensidad_e_impulso();
    return resumen("espectral.c");
}

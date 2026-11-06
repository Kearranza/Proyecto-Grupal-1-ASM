#include "prueba.h"

#define TOL_REL 1e-4 // float32 con N = 2048: error relativo esperado ~1e-6..1e-5

static double azar(void) { return 2.0 * rand() / RAND_MAX - 1.0; }

static int probar_n(int n, int compleja)
{
    static float re[N_MAX], im[N_MAX], x[N_MAX], xi[N_MAX];
    char nombre[64];
    snprintf(nombre, sizeof nombre, "fft contra DFT %s, n = %d", compleja ? "compleja" : "real", n);
    for (int i = 0; i < n; i++)
    {
        x[i] = (float)azar();
        xi[i] = compleja ? (float)azar() : 0.0f;
        re[i] = x[i];
        im[i] = xi[i];
    }
    int r = fft(re, im, n);
    if (es_pendiente(r, nombre, "nodo3"))
        return 0;
    if (r != ASM_OK)
    {
        FALLA(nombre, "devolvio %d", r);
        return 0;
    }

    double err_max = 0.0, mag_max = 0.0;
    for (int k = 0; k < n; k++)
    {
        double sr = 0.0, si = 0.0;
        for (int t = 0; t < n; t++)
        {
            double ang = -2.0 * M_PI * (double)k * t / n;
            sr += x[t] * cos(ang) - xi[t] * sin(ang);
            si += x[t] * sin(ang) + xi[t] * cos(ang);
        }
        double e = hypot(re[k] - sr, im[k] - si);
        if (e > err_max)
            err_max = e;
        if (hypot(sr, si) > mag_max)
            mag_max = hypot(sr, si);
    }
    if (err_max / mag_max > TOL_REL)
        FALLA(nombre, "error relativo %.2e", err_max / mag_max);
    else
        OK(nombre);

    snprintf(nombre, sizeof nombre, "ida y vuelta %s, n = %d", compleja ? "compleja" : "real", n);
    r = ifft(re, im, n);
    if (es_pendiente(r, nombre, "nodo3"))
        return 0;
    if (r != ASM_OK)
    {
        FALLA(nombre, "devolvio %d", r);
        return 0;
    }
    double e_ida = 0.0;
    for (int i = 0; i < n; i++)
        e_ida = fmax(e_ida, fabs(re[i] - x[i]) + fabs(im[i] - xi[i]));
    if (e_ida > 1e-4)
        FALLA(nombre, "error maximo %.2e", e_ida);
    else
        OK(nombre);
    return 0;
}

int main(void)
{
    printf("\n== fft.c ==\n");
    srand(1);
    int ns[] = {8, 16, 32, 64, 128, 256, 512, 1024, N_MAX};
    for (size_t i = 0; i < sizeof ns / sizeof ns[0]; i++)
    {
        probar_n(ns[i], 0);
        probar_n(ns[i], 1);
    }

    float re[16] = {0}, im[16] = {0};
    int r = fft(re, im, 12);
    if (!es_pendiente(r, "n que no es potencia de dos", "nodo3"))
    {
        if (r == ASM_ERROR)
            OK("n que no es potencia de dos");
        else
            FALLA("n que no es potencia de dos", "deberia devolver ASM_ERROR");
    }
    // Los errores no deben modificar los datos del llamador.
    int invalidos[] = {-8, 0, 1, 4, 12, N_MAX + 1, N_MAX * 2};
    for (int inversa = 0; inversa < 2; inversa++)
    {
        int (*transformada)(float *, float *, int) = inversa ? ifft : fft;
        re[0] = 3.0f;
        im[0] = -2.0f;
        for (size_t i = 0; i < sizeof invalidos / sizeof invalidos[0]; i++)
        {
            if (transformada(re, im, invalidos[i]) != ASM_ERROR || re[0] != 3.0f || im[0] != -2.0f)
                FALLA("argumentos invalidos", "inversa=%d, n=%d", inversa, invalidos[i]);
        }
        if (transformada(NULL, im, 8) != ASM_ERROR ||
            transformada(re, NULL, 8) != ASM_ERROR ||
            transformada(re, re, 8) != ASM_ERROR)
            FALLA("punteros invalidos", "inversa=%d", inversa);
        else
            OK(inversa ? "validacion IFFT" : "validacion FFT");
    }
    return resumen("fft.c");
}

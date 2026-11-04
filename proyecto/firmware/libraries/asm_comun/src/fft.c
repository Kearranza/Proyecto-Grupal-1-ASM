#include <math.h>
#include "config.h"
#include "fft.h"

static int transformar(float *re, float *im, int n, int inversa)
{
    // Validar antes de modificar los datos. los arreglos deben ser independientes.
    if (!re || !im || re == im || !n_es_valido(n))
        return ASM_ERROR;

    // La permutacion deja las parejas listas para las mariposas
    for (int i = 1, j = 0; i < n; i++)
    {
        int bit = n >> 1;
        while (j & bit)
        {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j)
        {
            float t = re[i];
            re[i] = re[j];
            re[j] = t;
            t = im[i];
            im[i] = im[j];
            im[j] = t;
        }
    }

    for (int largo = 2; largo <= n; largo <<= 1)
    {
        int mitad = largo / 2;
        float angulo = (inversa ? 6.2831853071795864769f : -6.2831853071795864769f) / largo;
        float paso_re = cosf(angulo), paso_im = sinf(angulo);
        float w_re = 1.0f, w_im = 0.0f;
        for (int j = 0; j < mitad; j++)
        {
            // Reutilizar cada giro en todos los bloques reduce el costo sin FPU.
            // Reiniciar cada 64 giros limita el error acumulado en float32.
            if (j && j % 64 == 0)
            {
                w_re = cosf(angulo * j);
                w_im = sinf(angulo * j);
            }
            for (int a = j; a < n; a += largo)
            {
                int b = a + mitad;
                float t_re = w_re * re[b] - w_im * im[b];
                float t_im = w_re * im[b] + w_im * re[b];
                re[b] = re[a] - t_re;
                im[b] = im[a] - t_im;
                re[a] += t_re;
                im[a] += t_im;
            }
            float siguiente_re = w_re * paso_re - w_im * paso_im;
            w_im = w_re * paso_im + w_im * paso_re;
            w_re = siguiente_re;
        }
    }

    // La directa no se escala. la inversa incluye el factor 1/N.
    if (inversa)
    {
        float escala = 1.0f / n;
        for (int i = 0; i < n; i++)
        {
            re[i] *= escala;
            im[i] *= escala;
        }
    }
    return ASM_OK;
}

int fft(float *re, float *im, int n)
{
    return transformar(re, im, n, 0);
}

int ifft(float *re, float *im, int n)
{
    return transformar(re, im, n, 1);
}

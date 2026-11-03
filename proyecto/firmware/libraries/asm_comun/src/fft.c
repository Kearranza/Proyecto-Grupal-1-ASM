// FFT/IFFT compartida (Gretchell). Base: src/dft_fft.py de la Tarea 1
// (permutacion bit-reverso + mariposas por etapas). Validar con pruebas_c/test_fft.c.
#include "config.h"
#include "fft.h"

int fft(float *re, float *im, int n) {
    (void)re; (void)im; (void)n;
    return ASM_PENDIENTE;  // TODO (Gretchell)
}

int ifft(float *re, float *im, int n) {
    (void)re; (void)im; (void)n;
    return ASM_PENDIENTE;  // TODO (Gretchell): conjugar, fft, conjugar y dividir entre n
}

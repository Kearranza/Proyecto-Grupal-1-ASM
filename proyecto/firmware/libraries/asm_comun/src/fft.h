// FFT/IFFT compartida (Gretchell). La usan el nodo 1 (Pico W) y el nodo 3 (ESP32).
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// Radix-2 en sitio. n potencia de dos entre 8 y N_MAX.
// Devuelve ASM_OK, ASM_ERROR si n no es valido, o ASM_PENDIENTE.
int fft(float *re, float *im, int n);
// Inversa con la escala 1/n incluida: ifft(fft(x)) == x
int ifft(float *re, float *im, int n);

#ifdef __cplusplus
}
#endif

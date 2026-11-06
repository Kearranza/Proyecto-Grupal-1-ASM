// Imprime en hexadecimal la trama que arma trama.c, para que pc/trama.py
// compruebe que las dos implementaciones producen exactamente los mismos bytes.
//   ./bin/volcar_trama <tipo> <seq> <carga_en_hex>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trama.h"

static int nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "uso: volcar_trama <tipo> <seq> <carga_hex>\n"); return 2; }
    uint8_t carga[TRAMA_CARGA_MAX];
    size_t largo = strlen(argv[3]) / 2;
    if (largo > TRAMA_CARGA_MAX) return 2;
    for (size_t i = 0; i < largo; i++) {
        int alto = nibble(argv[3][2*i]), bajo = nibble(argv[3][2*i + 1]);
        if (alto < 0 || bajo < 0) return 2;
        carga[i] = (uint8_t)((alto << 4) | bajo);
    }
    uint8_t salida[TRAMA_MAX_BYTES];
    int n = trama_armar((uint8_t)atoi(argv[1]), (uint16_t)atoi(argv[2]),
                        carga, (uint16_t)largo, salida, sizeof salida);
    if (n <= 0) return 1;
    for (int i = 0; i < n; i++) printf("%02x", salida[i]);
    printf("\n");
    return 0;
}

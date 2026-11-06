// Protocolo entre placas (Kevin). Especificacion completa en docs/protocolo.md.
//
//   | 0xAA | 0x55 | tipo | seq (u16) | largo (u16) | carga (largo bytes) | crc (u16) |
//
// Enteros little-endian. El CRC-16/CCITT-FALSE cubre tipo, seq, largo y carga.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "config.h"
#ifdef __cplusplus
extern "C" {
#endif

#define TRAMA_ENCABEZADO 7   // sync0 sync1 tipo seq_lo seq_hi largo_lo largo_hi
#define TRAMA_COLA 2         // crc_lo crc_hi
#define TRAMA_MAX_BYTES (TRAMA_ENCABEZADO + TRAMA_CARGA_MAX + TRAMA_COLA)

uint16_t crc16_ccitt(const uint8_t *datos, size_t largo);

// Arma una trama completa en 'salida'. Devuelve los bytes escritos,
// 0 si no cabe o si largo > TRAMA_CARGA_MAX, o ASM_PENDIENTE.
int trama_armar(uint8_t tipo, uint16_t seq, const uint8_t *carga, uint16_t largo,
                uint8_t *salida, size_t capacidad);

typedef struct {
    uint8_t tipo;
    uint16_t seq;
    uint16_t largo;
    uint8_t carga[TRAMA_CARGA_MAX];
} trama_t;

// Lector byte a byte para la UART. Lleva la cuenta de errores (requisitos 5.2 y 5.3).
typedef struct {
    int estado;
    size_t indice;
    uint8_t bufer[TRAMA_MAX_BYTES];
    int hay_seq_previo;
    uint16_t seq_previo;
    uint32_t tramas_ok;
    uint32_t errores_crc;
    uint32_t bloques_perdidos;   // suma de los saltos en seq
    uint32_t bytes_descartados;  // basura antes de encontrar 0xAA 0x55
} lector_trama_t;

void trama_lector_iniciar(lector_trama_t *lector);
// Devuelve 1 cuando 'b' completa una trama valida (copiada en 'salida'),
// 0 si todavia falta, o ASM_PENDIENTE.
int trama_leer_byte(lector_trama_t *lector, uint8_t b, trama_t *salida);

#ifdef __cplusplus
}
#endif

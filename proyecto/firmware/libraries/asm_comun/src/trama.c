// Protocolo entre placas (Kevin). Especificacion en docs/protocolo.md.
// Validar con pruebas_c/test_trama.c y contra pc/trama.py.
#include <string.h>
#include "trama.h"

// Estados del lector byte a byte
enum { BUSCAR_SYNC0, BUSCAR_SYNC1, ENCABEZADO, CARGA, CRC };

uint16_t crc16_ccitt(const uint8_t *datos, size_t largo) {
    // CRC-16/CCITT-FALSE: 0x1021, inicial 0xFFFF, sin reflejar, sin XOR final
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < largo; i++) {
        crc ^= (uint16_t)datos[i] << 8;
        for (int bit = 0; bit < 8; bit++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

int trama_armar(uint8_t tipo, uint16_t seq, const uint8_t *carga, uint16_t largo,
                uint8_t *salida, size_t capacidad) {
    size_t total = (size_t)TRAMA_ENCABEZADO + largo + TRAMA_COLA;
    if (!salida || largo > TRAMA_CARGA_MAX || capacidad < total) return 0;
    if (largo && !carga) return 0;

    salida[0] = TRAMA_SYNC0;
    salida[1] = TRAMA_SYNC1;
    salida[2] = tipo;
    salida[3] = (uint8_t)(seq & 0xFF);
    salida[4] = (uint8_t)(seq >> 8);
    salida[5] = (uint8_t)(largo & 0xFF);
    salida[6] = (uint8_t)(largo >> 8);
    if (largo) memcpy(salida + TRAMA_ENCABEZADO, carga, largo);

    // El CRC cubre tipo, seq, largo y carga; el sync queda fuera a proposito,
    // para que resincronizar no dependa de haber leido bien el encabezado
    uint16_t crc = crc16_ccitt(salida + 2, (size_t)5 + largo);
    salida[TRAMA_ENCABEZADO + largo] = (uint8_t)(crc & 0xFF);
    salida[TRAMA_ENCABEZADO + largo + 1] = (uint8_t)(crc >> 8);
    return (int)total;
}

void trama_lector_iniciar(lector_trama_t *lector) {
    memset(lector, 0, sizeof(*lector));
}

static void reiniciar(lector_trama_t *l) {
    l->estado = BUSCAR_SYNC0;
    l->indice = 0;
}

static uint16_t leer_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

int trama_leer_byte(lector_trama_t *lector, uint8_t b, trama_t *salida) {
    if (!lector || !salida) return 0;
    lector->bufer[lector->indice++] = b;

    switch (lector->estado) {
    case BUSCAR_SYNC0:
        if (b == TRAMA_SYNC0) {
            lector->estado = BUSCAR_SYNC1;
        } else {
            lector->bytes_descartados++;
            lector->indice = 0;
        }
        return 0;

    case BUSCAR_SYNC1:
        if (b == TRAMA_SYNC1) {
            lector->estado = ENCABEZADO;
        } else if (b == TRAMA_SYNC0) {
            // Este byte puede ser el sync0 de la trama de verdad
            lector->bytes_descartados++;
            lector->bufer[0] = b;
            lector->indice = 1;
        } else {
            lector->bytes_descartados += 2;
            reiniciar(lector);
        }
        return 0;

    case ENCABEZADO:
        if (lector->indice < TRAMA_ENCABEZADO) return 0;
        if (leer_u16(lector->bufer + 5) > TRAMA_CARGA_MAX) {
            // Largo imposible: la trama no era tal, se vuelve a buscar el sync
            lector->bytes_descartados += lector->indice;
            reiniciar(lector);
            return 0;
        }
        lector->estado = leer_u16(lector->bufer + 5) ? CARGA : CRC;
        return 0;

    case CARGA:
        if (lector->indice < (size_t)TRAMA_ENCABEZADO + leer_u16(lector->bufer + 5)) return 0;
        lector->estado = CRC;
        return 0;

    case CRC: {
        uint16_t largo = leer_u16(lector->bufer + 5);
        size_t total = (size_t)TRAMA_ENCABEZADO + largo + TRAMA_COLA;
        if (lector->indice < total) return 0;

        uint16_t esperado = crc16_ccitt(lector->bufer + 2, (size_t)5 + largo);
        uint16_t recibido = leer_u16(lector->bufer + TRAMA_ENCABEZADO + largo);
        reiniciar(lector);
        if (esperado != recibido) {
            lector->errores_crc++;
            return 0;
        }

        salida->tipo = lector->bufer[2];
        salida->seq = leer_u16(lector->bufer + 3);
        salida->largo = largo;
        if (largo) memcpy(salida->carga, lector->bufer + TRAMA_ENCABEZADO, largo);

        // Un salto en seq son bloques que no llegaron (requisito 5.3). La resta
        // en uint16 da el salto correcto aunque el contador haya dado la vuelta
        if (lector->hay_seq_previo)
            lector->bloques_perdidos += (uint16_t)(salida->seq - lector->seq_previo - 1);
        lector->hay_seq_previo = 1;
        lector->seq_previo = salida->seq;
        lector->tramas_ok++;
        return 1;
    }

    default:
        reiniciar(lector);
        return 0;
    }
}

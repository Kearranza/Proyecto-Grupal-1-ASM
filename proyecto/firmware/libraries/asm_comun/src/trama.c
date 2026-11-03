// Protocolo entre placas (Kevin). Especificacion en docs/protocolo.md.
// Validar con pruebas_c/test_trama.c y contra pc/trama.py.
#include <string.h>
#include "trama.h"

uint16_t crc16_ccitt(const uint8_t *datos, size_t largo) {
    (void)datos; (void)largo;
    return 0;  // TODO (Kevin): polinomio 0x1021, inicial 0xFFFF. "123456789" -> 0x29B1
}

int trama_armar(uint8_t tipo, uint16_t seq, const uint8_t *carga, uint16_t largo,
                uint8_t *salida, size_t capacidad) {
    (void)tipo; (void)seq; (void)carga; (void)largo; (void)salida; (void)capacidad;
    return ASM_PENDIENTE;  // TODO (Kevin)
}

void trama_lector_iniciar(lector_trama_t *lector) {
    memset(lector, 0, sizeof(*lector));
}

int trama_leer_byte(lector_trama_t *lector, uint8_t b, trama_t *salida) {
    // TODO (Kevin): maquina de estados BUSCAR_SYNC0 -> SYNC1 -> ENCABEZADO -> CARGA -> CRC.
    // Si el CRC falla: errores_crc++ y volver a buscar 0xAA 0x55 desde el byte siguiente.
    // Si seq salta: bloques_perdidos += (seq - seq_previo - 1).
    (void)lector; (void)b; (void)salida;
    return ASM_PENDIENTE;
}

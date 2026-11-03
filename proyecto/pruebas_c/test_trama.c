// Contrato de trama.c (Kevin). Cubre los requisitos 5.2 (delimitacion y deteccion
// de errores) y 5.3 (bloques perdidos) del proyecto.
#include <string.h>
#include "prueba.h"

static uint8_t buf[TRAMA_MAX_BYTES];
static trama_t t;

// Pasa 'largo' bytes al lector y devuelve cuantas tramas completas salieron
static int alimentar(lector_trama_t *l, const uint8_t *b, int largo) {
    int completas = 0;
    for (int i = 0; i < largo; i++) completas += trama_leer_byte(l, b[i], &t) == 1;
    return completas;
}

int main(void) {
    printf("\n== trama.c ==\n");
    uint8_t carga[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    int largo = trama_armar(TRAMA_TIPO_MUESTRAS, 0x1234, carga, sizeof carga, buf, sizeof buf);
    if (es_pendiente(largo, "armar trama", "Kevin")) return resumen("trama.c");

    const uint8_t vector[] = "123456789";
    uint16_t crc = crc16_ccitt(vector, 9);
    if (crc == 0x29B1) OK("CRC-16/CCITT-FALSE de \"123456789\" = 0x29B1");
    else FALLA("CRC-16/CCITT-FALSE", "dio 0x%04X, se esperaba 0x29B1", crc);

    if (largo == TRAMA_ENCABEZADO + 8 + TRAMA_COLA && buf[0] == TRAMA_SYNC0 && buf[1] == TRAMA_SYNC1 &&
        buf[2] == TRAMA_TIPO_MUESTRAS && buf[3] == 0x34 && buf[4] == 0x12 && buf[5] == 8 && buf[6] == 0)
        OK("formato: sync, tipo, seq y largo little-endian");
    else FALLA("formato", "encabezado distinto al de docs/protocolo.md");

    if (trama_armar(1, 0, carga, TRAMA_CARGA_MAX + 1, buf, sizeof buf) == 0) OK("rechaza carga demasiado grande");
    else FALLA("carga demasiado grande", "deberia devolver 0");

    lector_trama_t l;
    trama_lector_iniciar(&l);
    largo = trama_armar(TRAMA_TIPO_MUESTRAS, 7, carga, sizeof carga, buf, sizeof buf);
    int r = trama_leer_byte(&l, buf[0], &t);
    if (es_pendiente(r, "leer trama", "Kevin")) return resumen("trama.c");
    trama_lector_iniciar(&l);
    if (alimentar(&l, buf, largo) == 1 && t.seq == 7 && t.largo == 8 && memcmp(t.carga, carga, 8) == 0)
        OK("ida y vuelta");
    else FALLA("ida y vuelta", "la trama leida no coincide con la enviada");

    trama_lector_iniciar(&l);
    uint8_t basura[] = {0x00, 0xAA, 0x13, 0xFF};
    alimentar(&l, basura, sizeof basura);
    if (alimentar(&l, buf, largo) == 1) OK("se resincroniza despues de basura");
    else FALLA("resincronizacion", "no encontro la trama despues de bytes basura");

    trama_lector_iniciar(&l);
    buf[TRAMA_ENCABEZADO + 3] ^= 0x01;   // un bit cambiado en la carga
    if (alimentar(&l, buf, largo) == 0 && l.errores_crc == 1) OK("detecta un bit cambiado (CRC)");
    else FALLA("deteccion de errores", "acepto una trama corrupta o no conto el error");

    trama_lector_iniciar(&l);
    for (uint16_t seq = 10; seq <= 14; seq++) {
        if (seq == 12) continue;   // se "pierde" el bloque 12
        largo = trama_armar(TRAMA_TIPO_MUESTRAS, seq, carga, sizeof carga, buf, sizeof buf);
        alimentar(&l, buf, largo);
    }
    if (l.bloques_perdidos == 1 && l.tramas_ok == 4) OK("cuenta bloques perdidos por salto de seq");
    else FALLA("bloques perdidos", "perdidos = %u, ok = %u (se esperaba 1 y 4)",
               (unsigned)l.bloques_perdidos, (unsigned)l.tramas_ok);

    return resumen("trama.c");
}

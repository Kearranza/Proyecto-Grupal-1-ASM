"""Protocolo entre placas en Python (Kevin). Misma especificacion que trama.c.

Sirve para dos cosas:
  - leer desde la computadora lo que mandan las placas (registrar.py);
  - validar trama.c: las dos implementaciones deben producir los mismos bytes.

    python proyecto/pc/trama.py      # autoprueba
"""
import json
import struct
from pathlib import Path

SISTEMA = json.loads((Path(__file__).resolve().parent.parent / 'config' / 'sistema.json')
                     .read_text(encoding='utf-8'))
SYNC = bytes(SISTEMA['enlace']['sync'])
TIPOS = SISTEMA['enlace']['tipos']
CARGA_MAX = SISTEMA['enlace']['carga_max_bytes']
ENCABEZADO = struct.Struct('<2sBHH')   # sync, tipo, seq, largo
DIAGNOSTICO = struct.Struct('<BBHfff')  # igual que diagnostico_t (16 bytes)


def crc16_ccitt(datos: bytes) -> int:
    """CRC-16/CCITT-FALSE: polinomio 0x1021, inicial 0xFFFF. b'123456789' -> 0x29B1."""
    raise NotImplementedError('TODO (Kevin): crc16_ccitt')


def armar(tipo: int, seq: int, carga: bytes) -> bytes:
    raise NotImplementedError('TODO (Kevin): armar')


class Lector:
    """Lector incremental: se le pasan bytes como lleguen y devuelve tramas completas."""

    def __init__(self):
        self.tramas_ok = self.errores_crc = self.bloques_perdidos = 0
        self.seq_previo = None

    def alimentar(self, datos: bytes):
        """Devuelve una lista de (tipo, seq, carga) validas."""
        raise NotImplementedError('TODO (Kevin): Lector.alimentar')


def autoprueba():
    from pasos import ejecutar

    def crc():
        assert crc16_ccitt(b'123456789') == 0x29B1

    def ida_y_vuelta():
        t = armar(TIPOS['muestras'], 7, bytes(range(8)))
        assert Lector().alimentar(t) == [(TIPOS['muestras'], 7, bytes(range(8)))]

    ejecutar('pc/trama.py', [('CRC conocido', crc), ('ida y vuelta', ida_y_vuelta)])


if __name__ == '__main__':
    autoprueba()

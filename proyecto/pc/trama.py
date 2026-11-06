"""Protocolo entre placas en Python (Kevin). Misma especificacion que trama.c.

Sirve para dos cosas:
  - leer desde la computadora lo que mandan las placas (registrar.py);
  - validar trama.c: las dos implementaciones deben producir los mismos bytes.

    python proyecto/pc/trama.py      # autoprueba
"""
import json
import struct
import subprocess
import sys
from pathlib import Path

PROYECTO = Path(__file__).resolve().parent.parent
SISTEMA = json.loads((PROYECTO / 'config' / 'sistema.json').read_text(encoding='utf-8'))
SYNC = bytes(SISTEMA['enlace']['sync'])
TIPOS = SISTEMA['enlace']['tipos']
CARGA_MAX = SISTEMA['enlace']['carga_max_bytes']
ENCABEZADO = struct.Struct('<2sBHH')   # sync, tipo, seq, largo
DIAGNOSTICO = struct.Struct('<BBHfff')  # igual que diagnostico_t (16 bytes)

TAM_ENCABEZADO = ENCABEZADO.size   # 7
TAM_COLA = 2


def crc16_ccitt(datos: bytes) -> int:
    """CRC-16/CCITT-FALSE: polinomio 0x1021, inicial 0xFFFF. b'123456789' -> 0x29B1."""
    crc = 0xFFFF
    for byte in datos:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def armar(tipo: int, seq: int, carga: bytes) -> bytes:
    if len(carga) > CARGA_MAX:
        raise ValueError(f'carga de {len(carga)} B, el maximo es {CARGA_MAX}')
    cuerpo = struct.pack('<BHH', tipo, seq & 0xFFFF, len(carga)) + carga
    return SYNC + cuerpo + struct.pack('<H', crc16_ccitt(cuerpo))


class Lector:
    """Lector incremental: se le pasan bytes como lleguen y devuelve tramas completas."""

    def __init__(self):
        self.tramas_ok = self.errores_crc = self.bloques_perdidos = 0
        self.bytes_descartados = 0
        self.seq_previo = None
        self._bufer = bytearray()

    def alimentar(self, datos: bytes):
        """Devuelve una lista de (tipo, seq, carga) validas."""
        self._bufer.extend(datos)
        completas = []
        while True:
            inicio = self._bufer.find(SYNC)
            if inicio < 0:
                # Puede quedar un 0xAA suelto que sea el sync de la proxima trama
                sobra = len(self._bufer) - (1 if self._bufer.endswith(SYNC[:1]) else 0)
                self.bytes_descartados += sobra
                del self._bufer[:sobra]
                return completas
            if inicio:
                self.bytes_descartados += inicio
                del self._bufer[:inicio]
            if len(self._bufer) < TAM_ENCABEZADO:
                return completas

            _, tipo, seq, largo = ENCABEZADO.unpack_from(self._bufer)
            if largo > CARGA_MAX:
                self.bytes_descartados += 1
                del self._bufer[:1]
                continue
            total = TAM_ENCABEZADO + largo + TAM_COLA
            if len(self._bufer) < total:
                return completas

            cuerpo = bytes(self._bufer[2:TAM_ENCABEZADO + largo])
            recibido, = struct.unpack_from('<H', self._bufer, TAM_ENCABEZADO + largo)
            del self._bufer[:total]
            if crc16_ccitt(cuerpo) != recibido:
                self.errores_crc += 1
                continue

            if self.seq_previo is not None:
                self.bloques_perdidos += (seq - self.seq_previo - 1) & 0xFFFF
            self.seq_previo = seq
            self.tramas_ok += 1
            completas.append((tipo, seq, cuerpo[5:]))


def _trama_desde_c(tipo: int, seq: int, carga: bytes):
    """Pide la misma trama a trama.c. Devuelve None si no se puede compilar."""
    pruebas = PROYECTO / 'pruebas_c'
    binario = pruebas / 'bin' / 'volcar_trama'
    if not binario.exists():
        if subprocess.run(['make', '-s', 'bin/volcar_trama'], cwd=pruebas,
                          capture_output=True).returncode:
            return None
    salida = subprocess.run([str(binario), str(tipo), str(seq), carga.hex()],
                            capture_output=True, text=True)
    return bytes.fromhex(salida.stdout.strip()) if salida.returncode == 0 else None


def autoprueba():
    sys.path.insert(0, str(PROYECTO / 'pc'))
    from pasos import ejecutar

    carga = bytes(range(8))

    def crc():
        assert crc16_ccitt(b'123456789') == 0x29B1

    def ida_y_vuelta():
        t = armar(TIPOS['muestras'], 7, carga)
        assert Lector().alimentar(t) == [(TIPOS['muestras'], 7, carga)]

    def formato():
        t = armar(TIPOS['muestras'], 0x1234, carga)
        assert t[:2] == SYNC and t[2] == TIPOS['muestras']
        assert t[3:5] == b'\x34\x12' and t[5:7] == b'\x08\x00'
        assert len(t) == TAM_ENCABEZADO + 8 + TAM_COLA

    def resincroniza():
        lector = Lector()
        lector.alimentar(bytes([0x00, 0xAA, 0x13, 0xFF]))
        assert len(lector.alimentar(armar(TIPOS['muestras'], 1, carga))) == 1

    def detecta_error():
        t = bytearray(armar(TIPOS['muestras'], 1, carga))
        t[TAM_ENCABEZADO + 3] ^= 0x01
        lector = Lector()
        assert lector.alimentar(bytes(t)) == [] and lector.errores_crc == 1

    def cuenta_perdidos():
        lector = Lector()
        for seq in (10, 11, 13, 14):
            lector.alimentar(armar(TIPOS['muestras'], seq, carga))
        assert lector.bloques_perdidos == 1 and lector.tramas_ok == 4

    def da_la_vuelta():
        lector = Lector()
        for seq in (65534, 65535, 1):   # se pierde el 0
            lector.alimentar(armar(TIPOS['muestras'], seq, carga))
        assert lector.bloques_perdidos == 1, lector.bloques_perdidos

    def troceado():
        # La UART entrega los bytes de a poco: el lector tiene que aguantarlo
        t = armar(TIPOS['muestras'], 3, carga)
        lector, completas = Lector(), []
        for i in range(len(t)):
            completas += lector.alimentar(t[i:i + 1])
        assert completas == [(TIPOS['muestras'], 3, carga)]

    def igual_que_c():
        esperado = _trama_desde_c(TIPOS['muestras'], 0x1234, carga)
        if esperado is None:
            raise NotImplementedError('no se pudo compilar volcar_trama (falta gcc o make)')
        assert armar(TIPOS['muestras'], 0x1234, carga) == esperado, 'Python y C no coinciden'

    ejecutar('pc/trama.py', [
        ('CRC conocido', crc),
        ('formato de la trama', formato),
        ('ida y vuelta', ida_y_vuelta),
        ('se resincroniza despues de basura', resincroniza),
        ('detecta un bit cambiado', detecta_error),
        ('cuenta bloques perdidos', cuenta_perdidos),
        ('cuenta bien cuando seq da la vuelta', da_la_vuelta),
        ('acepta los bytes troceados', troceado),
        ('mismos bytes que trama.c', igual_que_c),
    ])


if __name__ == '__main__':
    autoprueba()

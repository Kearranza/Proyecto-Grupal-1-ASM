"""BLOQUE C - Control del emisor por serial (sin recompilar).

Lee los modos de parametros.json y se los manda a la Pico con el protocolo
documentado en emisor_pico.ino. Asi, si en la defensa cambian una frecuencia,
se edita el JSON y se vuelve a correr esto.

    pip install pyserial
    python taller_semana12/c_emisor/control_emisor.py --puerto COM5 --modo 3
    python taller_semana12/c_emisor/control_emisor.py --puerto /dev/ttyUSB0 --estado
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comun import parametros as P  # noqa: E402


def ordenes_del_modo(m):
    """Lista de lineas TONO ... que reproducen el modo m del JSON."""
    # TODO (C): una linea 'TONO i f amp' por componente, y apagar las que sobren
    raise NotImplementedError('TODO (C): armar las ordenes del modo')


def enviar(puerto, baudios, lineas):
    """Abrir el puerto, mandar cada linea y mostrar la respuesta (OK / ERR)."""
    # TODO (C): con serial.Serial(puerto, baudios, timeout=1)
    raise NotImplementedError('TODO (C): enviar por serial')


def main():
    p = P.cargar()
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--puerto', required=True)
    ap.add_argument('--modo', type=int)
    ap.add_argument('--estado', action='store_true')
    args = ap.parse_args()

    lineas = ['ESTADO'] if args.estado else ordenes_del_modo(P.modo(p, args.modo))
    enviar(args.puerto, p['emisor']['baudios'], lineas)


if __name__ == '__main__':
    main()

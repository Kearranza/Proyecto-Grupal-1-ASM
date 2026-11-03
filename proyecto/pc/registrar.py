"""Registra por serial lo que mandan las placas, como evidencia de las pruebas (Kevin).

    pip install pyserial
    python proyecto/pc/registrar.py --puerto COM7 --segundos 10 --salida proyecto/resultados/p1_r1

Guarda las tramas decodificadas (muestras, diagnostico, estado) y los contadores
del lector (errores de CRC, bloques perdidos) para el paper.
"""
import argparse


def registrar(puerto, segundos, salida):
    raise NotImplementedError('TODO (Kevin): leer con serial.Serial y trama.Lector, guardar CSV/NPZ')


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--puerto', required=True)
    ap.add_argument('--segundos', type=float, default=10.0)
    ap.add_argument('--salida', required=True)
    a = ap.parse_args()
    registrar(a.puerto, a.segundos, a.salida)


if __name__ == '__main__':
    main()

"""Manda comandos a los nodos sin recompilar (Kevin). Clave para la prueba P6.

    python proyecto/pc/comandos.py --puerto COM7 N 512
    python proyecto/pc/comandos.py --puerto COM8 UMBRAL 0.3
    python proyecto/pc/comandos.py --puerto COM8 ESTADO

Los comandos que entiende cada nodo estan al inicio de su .ino.
"""
import argparse


def enviar(puerto, orden):
    raise NotImplementedError('TODO (Kevin): abrir el puerto, mandar la linea y mostrar la respuesta')


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--puerto', required=True)
    ap.add_argument('orden', nargs='+')
    a = ap.parse_args()
    enviar(a.puerto, ' '.join(a.orden))


if __name__ == '__main__':
    main()

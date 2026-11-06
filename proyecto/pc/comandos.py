"""Manda comandos a los nodos sin recompilar (Kevin). Clave para la prueba P6.

    python proyecto/pc/comandos.py --puerto COM7 N 512
    python proyecto/pc/comandos.py --puerto COM8 UMBRAL 0.3
    python proyecto/pc/comandos.py --puerto COM8 ESTADO
    python proyecto/pc/comandos.py --autoprueba   # revisa la validacion sin placa

Los comandos que entiende cada nodo estan al inicio de su .ino.
"""
import argparse
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import trama as T  # noqa: E402

# Lo que se puede cambiar en vivo, segun config/sistema.json
AJUSTABLES = {'N': 'n_bloque', 'UMBRAL': 'umbral_espectral',
              'METODO': 'metodo_temporal', 'ANCHO': 'ancho_rechazo_hz'}
N_PERMITIDOS = T.SISTEMA['bloque']['n_permitidos']


def revisar(orden):
    # Avisar antes de mandar algo que el nodo va a rechazar
    partes = orden.split()
    if partes[0] == 'N' and len(partes) > 1:
        if not partes[1].isdigit() or int(partes[1]) not in N_PERMITIDOS:
            raise SystemExit(f'N tiene que ser uno de {N_PERMITIDOS}')
    if partes[0] not in AJUSTABLES and partes[0] != 'ESTADO':
        print(f"  aviso: '{partes[0]}' no esta en los ajustables "
              f"({', '.join(AJUSTABLES)}); se manda igual")


def enviar(puerto, orden, espera=1.0):
    revisar(orden)
    try:
        import serial
    except ImportError:
        raise SystemExit('falta pyserial: pip install pyserial')
    with serial.Serial(puerto, T.SISTEMA['enlace']['baudios'], timeout=0.2) as puerto_serie:
        puerto_serie.reset_input_buffer()
        puerto_serie.write((orden + '\n').encode('ascii'))
        puerto_serie.flush()
        fin, respuesta = time.time() + espera, b''
        while time.time() < fin:
            respuesta += puerto_serie.read(256)
            if b'\n' in respuesta:
                break
    texto = respuesta.decode('ascii', 'replace').strip()
    print(f'  -> {orden}')
    print(f'  <- {texto}' if texto else '  <- (sin respuesta)')
    return texto


def autoprueba():
    """Revisa la validacion de ordenes sin necesidad de un puerto serie."""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from pasos import ejecutar

    def acepta(orden):
        return lambda: revisar(orden)

    def rechaza(orden):
        def comprobar():
            try:
                revisar(orden)
            except SystemExit:
                return
            raise AssertionError(f"'{orden}' deberia haber sido rechazada")
        return comprobar

    ejecutar('Comandos: validacion de ordenes', [
        (f'acepta N {N_PERMITIDOS[0]}', acepta(f'N {N_PERMITIDOS[0]}')),
        (f'acepta N {N_PERMITIDOS[-1]}', acepta(f'N {N_PERMITIDOS[-1]}')),
        ('rechaza N 777 (no esta en n_permitidos)', rechaza('N 777')),
        ('rechaza N abc', rechaza('N abc')),
        ('acepta UMBRAL 0.3', acepta('UMBRAL 0.3')),
        ('acepta ESTADO', acepta('ESTADO')),
        ('avisa de una orden desconocida pero la manda', acepta('XYZ 1')),
    ])


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--puerto')
    ap.add_argument('--espera', type=float, default=1.0)
    ap.add_argument('--autoprueba', action='store_true')
    ap.add_argument('orden', nargs='*')
    a = ap.parse_args()
    if a.autoprueba:
        autoprueba()
        return
    if not a.puerto or not a.orden:
        raise SystemExit('se necesitan --puerto y una orden (o --autoprueba)')
    enviar(a.puerto, ' '.join(a.orden), a.espera)


if __name__ == '__main__':
    main()

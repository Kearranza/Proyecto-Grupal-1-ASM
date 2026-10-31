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
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comun import parametros as P  # noqa: E402

MAX_COMPONENTES = 4   # = MAX_COMPONENTES en emisor_pico.ino


def ordenes_del_modo(m):
    """Lista de lineas TONO ... que reproducen el modo m del JSON.

    Una linea 'TONO i f amp' por componente del modo (i = 0, 1, ...). Las
    componentes que sobren se apagan con amplitud 0, para que no quede
    encendida alguna del modo anterior.
    """
    frecuencias, amplitudes = m['frecuencias_hz'], m['amplitudes']
    if len(frecuencias) != len(amplitudes):
        raise ValueError('frecuencias_hz y amplitudes deben tener el mismo largo')
    if len(frecuencias) > MAX_COMPONENTES:
        raise ValueError(f'El emisor admite {MAX_COMPONENTES} componentes y el modo '
                         f'{m["id"]} tiene {len(frecuencias)}')
    lineas = [f'TONO {i} {f:g} {a:g}'
              for i, (f, a) in enumerate(zip(frecuencias, amplitudes))]
    lineas += [f'TONO {i} 0 0' for i in range(len(frecuencias), MAX_COMPONENTES)]
    return lineas


def enviar(puerto, baudios, lineas):
    """Abrir el puerto, mandar cada linea y mostrar la respuesta (OK / ERR)."""
    try:
        import serial
    except ImportError:
        sys.exit('Falta pyserial: pip install pyserial')

    hubo_error = False
    with serial.Serial(puerto, baudios, timeout=1) as s:
        time.sleep(0.2)
        s.reset_input_buffer()                 # descarta el "OK emisor listo..." inicial
        for linea in lineas:
            s.write((linea + '\n').encode())
            respuesta = s.readline().decode(errors='ignore').strip()
            print(f'> {linea:<24s} < {respuesta or "(sin respuesta)"}')
            if not respuesta.startswith('OK'):
                hubo_error = True
    if hubo_error:
        sys.exit('La Pico rechazo alguna orden (ver arriba)')


def main():
    p = P.cargar()
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--puerto', required=True)
    ap.add_argument('--modo', type=int)
    ap.add_argument('--estado', action='store_true')
    args = ap.parse_args()
    if not args.estado and args.modo is None:
        ap.error('indica --modo N o --estado')

    lineas = ['ESTADO'] if args.estado else ordenes_del_modo(P.modo(p, args.modo))
    enviar(args.puerto, p['emisor']['baudios'], lineas)


if __name__ == '__main__':
    main()
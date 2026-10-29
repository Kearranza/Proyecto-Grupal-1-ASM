"""BLOQUE D - Comparacion de la FFT del micro contra la referencia de Python.

Para el taller basta con dejar definida la comparacion (la usan los diagramas
02 y 04). Para el proyecto, este script recibe el espectro que manda la Pico W
y decide si el bloque pasa o no con los margenes de parametros.json.

    python taller_semana12/d_validacion/comparar_fft.py espectro_pico.csv --modo 2
"""
import argparse
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comun import parametros as P  # noqa: E402
from comun.pasos import ejecutar  # noqa: E402

P.usar_src()


def margenes(p):
    """Leer y validar los margenes. Deben estar definidos antes de medir."""
    v = p['validacion']
    faltan = [k for k in ('margen_frecuencia_bins', 'margen_amplitud_db',
                          'snr_minima_captura_db') if v.get(k) is None]
    if faltan:
        raise NotImplementedError(f'TODO (D): definir en parametros.json {faltan}')
    return v


def espectro_referencia(p, identificador):
    """|X[k]| en dB de senal_del_modo() con la misma ventana y N que el micro."""
    raise NotImplementedError('TODO (D): espectro de referencia (usa la funcion de A)')


def leer_espectro_micro(ruta):
    """Espectro que reporta la Pico W (formato a definir con el diagrama 03)."""
    raise NotImplementedError('TODO (D): leer el espectro del micro')


def decidir(referencia, medido, v):
    """Pasa / no pasa por cada pico, con el error en bins y en dB."""
    raise NotImplementedError('TODO (D): regla de decision')


def main():
    p = P.cargar()
    ap = argparse.ArgumentParser()
    ap.add_argument('espectro', nargs='?')
    ap.add_argument('--modo', type=int, default=2)
    args = ap.parse_args()

    estado = {}
    ejecutar('Bloque D - validacion del receptor', [
        ('margenes definidos', lambda: estado.update(v=margenes(p))),
        ('espectro de referencia', lambda: estado.update(ref=espectro_referencia(p, args.modo))),
        ('espectro del micro', lambda: estado.update(med=leer_espectro_micro(args.espectro))),
        ('decision', lambda: decidir(estado['ref'], estado['med'], estado['v'])),
    ], encadenado=True)


if __name__ == '__main__':
    main()

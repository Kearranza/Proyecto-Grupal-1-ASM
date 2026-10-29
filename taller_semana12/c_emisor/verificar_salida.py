"""BLOQUE C - Verifica que la salida de la Pico tiene las frecuencias pedidas.

Evidencia del punto 2 del taller. La captura puede venir de:
  - el osciloscopio (CSV con columnas tiempo, voltaje), o
  - una grabacion de la salida filtrada (WAV).
Se calcula su espectro y se compara contra senal_del_modo() del bloque A.

    python taller_semana12/c_emisor/verificar_salida.py captura.csv --modo 2

Entrega de C en el PDF (informe/secciones/3_emisor.tex):
  1. Por que DDS + tabla + PWM, como se obtiene fs exacta, tamano de tabla, Q15 y error de frecuencia esperado.
  2. Diagrama de conexion Pico (GPIO15) -> filtro RC -> salida, con valores de R y C.
  3. Filtro de reconstruccion: fc, por que esa fc, atenuacion de la portadora PWM.
  4. Figura: espectro medido vs esperado para cada modo, con el error en Hz y dB.
"""
import argparse
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comun import parametros as P  # noqa: E402
from comun.pasos import ejecutar  # noqa: E402

P.usar_src()


def leer_captura(ruta):
    """Devuelve (x, fs) desde CSV del osciloscopio o WAV."""
    raise NotImplementedError('TODO (C): leer CSV o WAV')


def picos(x, fs, cuantos):
    """Frecuencia y nivel (dB) de los 'cuantos' picos mayores, con ventana Hann."""
    raise NotImplementedError('TODO (C): encontrar los picos del espectro')


def comparar(medidos, m):
    """Error en Hz y dB de cada componente medida contra el modo m del JSON."""
    raise NotImplementedError('TODO (C): comparar contra lo esperado')


def main():
    p = P.cargar()
    ap = argparse.ArgumentParser()
    ap.add_argument('captura', nargs='?')
    ap.add_argument('--modo', type=int, default=2)
    args = ap.parse_args()
    m = P.modo(p, args.modo)

    estado = {}
    ejecutar(f'Bloque C - verificacion del modo {m["id"]}', [
        ('leer captura', lambda: estado.update(zip(('x', 'fs'), leer_captura(args.captura)))),
        ('picos', lambda: estado.update(picos=picos(estado['x'], estado['fs'],
                                                    len(m['frecuencias_hz'])))),
        ('comparar', lambda: comparar(estado['picos'], m)),
    ], encadenado=True)


if __name__ == '__main__':
    main()

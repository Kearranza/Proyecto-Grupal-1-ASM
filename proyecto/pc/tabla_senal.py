"""Convierte los WAV de una prueba en tablas de C para la emisora (Kevin).

La ruta es acustica: emisora -> RC -> amplificador -> parlante -> aire -> microfono
-> ADC. Como el cuarto y el parlante colorean lo que llega, la referencia de las
metricas no puede ser el WAV: se graba una pasada con la senal limpia y otra con la
contaminada, por el mismo camino. Por eso van las dos tablas en el mismo flash, y se
elige por comando sin recompilar.

    python proyecto/pc/tabla_senal.py p3            # 4 s por defecto
    python proyecto/pc/tabla_senal.py p4 --segundos 8

Escribe firmware/sonda_emisor/tabla_senal.h
"""
import argparse
import sys
from pathlib import Path

import numpy as np

PROYECTO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROYECTO.parent / 'src'))
import senales  # noqa: E402

DESTINO = PROYECTO / 'firmware' / 'sonda_emisor' / 'tabla_senal.h'
POR_LINEA = 16


def a_pwm(x):
    # 128 es el reposo de la portadora, o sea el silencio
    return np.clip(np.rint(127.5 + 127.5*x), 0, 255).astype(np.uint8)


def cargar(carpeta, nombre, muestras):
    ruta = carpeta / f'{nombre}.wav'
    if not ruta.exists():
        raise SystemExit(f'falta {ruta.relative_to(PROYECTO.parent)}; '
                         f'correr antes pc/generar_wav_pruebas.py')
    x, fs = senales.leer_wav(ruta)
    return a_pwm(x[:muestras]), fs, float(np.max(np.abs(x[:muestras])))


def volcar(nombre, niveles):
    lineas = [f'const uint8_t {nombre}[TABLA_N] = {{']
    for i in range(0, niveles.size, POR_LINEA):
        lineas.append('    ' + ','.join(f'{v:3d}' for v in niveles[i:i+POR_LINEA]) + ',')
    return lineas + ['};', '']


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('prueba', nargs='?', default='p3', help='p3, p4 o p5')
    ap.add_argument('--segundos', type=float, default=4.0)
    a = ap.parse_args()

    carpeta = PROYECTO / 'senales' / a.prueba
    if not carpeta.is_dir():
        raise SystemExit(f'no existe {carpeta.relative_to(PROYECTO.parent)}')
    cabeza, fs, _ = cargar(carpeta, 'limpia', 1)
    muestras = int(round(a.segundos*fs))

    limpia, fs, pico_l = cargar(carpeta, 'limpia', muestras)
    sucia, _, pico_c = cargar(carpeta, 'contaminada', muestras)
    if limpia.size != sucia.size:
        raise SystemExit('limpia y contaminada tienen largos distintos')

    lineas = [f'// Generado por pc/tabla_senal.py desde senales/{a.prueba}/. No editar a mano.',
              '// Dos tablas: una pasada con la senal limpia da la referencia de las metricas,',
              '// otra con la contaminada da la entrada. El mismo camino acustico para las dos.',
              '#pragma once', '#include <stdint.h>', '',
              f'#define TABLA_PRUEBA "{a.prueba}"',
              f'#define TABLA_N {limpia.size}',
              f'#define TABLA_FS_HZ {int(fs)}',
              f'// picos del WAV: limpia {pico_l:.3f}, contaminada {pico_c:.3f}', '']
    lineas += volcar('TABLA_LIMPIA', limpia) + volcar('TABLA_CONTAMINADA', sucia)
    DESTINO.write_text('\n'.join(lineas), encoding='utf-8')

    print(f'  {a.prueba}: {limpia.size} muestras a {int(fs)} Hz = {limpia.size/fs:.3f} s')
    print(f'  picos: limpia {pico_l:.3f}, contaminada {pico_c:.3f}')
    print(f'  dos tablas de {limpia.size/1024:.0f} kB = {2*limpia.size/1024:.0f} kB de los 2048 kB de flash')
    print(f'  {DESTINO.relative_to(PROYECTO.parent)}')


if __name__ == '__main__':
    main()

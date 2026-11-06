"""Convierte un WAV en una tabla de C para que la emisora lo reproduzca (Kevin).

Asi P3 no necesita la tarjeta de sonido: la senal sale de la Pico por el mismo PWM
y el mismo RC que P1 y P2, es identica bit a bit en cada repeticion y el WAV de
origen sigue siendo la referencia con la que campana.py calcula las metricas.

    python proyecto/pc/tabla_senal.py senales/p3.wav

Escribe firmware/emisor_tabla/tabla_senal.h
"""
import sys
from pathlib import Path

import numpy as np

PROYECTO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROYECTO.parent / 'src'))
import senales  # noqa: E402

DESTINO = PROYECTO / 'firmware' / 'emisor_tabla' / 'tabla_senal.h'
POR_LINEA = 16


def convertir(ruta):
    """A PWM de 8 bits: el silencio queda en 128, como la portadora en reposo."""
    x, fs = senales.leer_wav(ruta)
    pico = float(np.max(np.abs(x)))
    niveles = np.clip(np.rint(127.5 + 127.5*x), 0, 255).astype(np.uint8)
    return niveles, fs, pico


def escribir(niveles, fs, origen, pico):
    lineas = [f'// Generado por pc/tabla_senal.py desde {origen}. No editar a mano.',
              '// Niveles de PWM de 8 bits: 128 es el reposo de la portadora.',
              '#pragma once', '#include <stdint.h>', '',
              f'#define TABLA_N {niveles.size}',
              f'#define TABLA_FS_HZ {int(fs)}',
              f'// pico del WAV: {pico:.3f}  ->  niveles de {niveles.min()} a {niveles.max()}',
              '', 'const uint8_t TABLA_SENAL[TABLA_N] = {']
    for i in range(0, niveles.size, POR_LINEA):
        trozo = ','.join(f'{v:3d}' for v in niveles[i:i+POR_LINEA])
        lineas.append(f'    {trozo},')
    lineas += ['};', '']
    DESTINO.parent.mkdir(parents=True, exist_ok=True)
    DESTINO.write_text('\n'.join(lineas), encoding='utf-8')


def main():
    ruta = PROYECTO / (sys.argv[1] if len(sys.argv) > 1 else 'senales/p3.wav')
    if not ruta.exists():
        raise SystemExit(f'no existe {ruta}; correr antes pc/generar_p3.py')
    niveles, fs, pico = convertir(ruta)
    escribir(niveles, fs, ruta.name, pico)
    print(f'  {ruta.name}: {niveles.size} muestras a {int(fs)} Hz, pico {pico:.3f}')
    print(f'  niveles de {niveles.min()} a {niveles.max()} (reposo 128)')
    print(f'  {DESTINO.relative_to(PROYECTO.parent)}: {DESTINO.stat().st_size/1024:.0f} kB de fuente, '
          f'{niveles.size/1024:.0f} kB en flash')
    print(f'  duracion {niveles.size/fs:.3f} s')


if __name__ == '__main__':
    main()

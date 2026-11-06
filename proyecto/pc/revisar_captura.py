"""Revisa la calidad de una captura antes de darla por buena (Kevin).

Los contadores de trama no detectan los dos problemas que arruinan una medicion sin
avisar: nivel insuficiente y distorsion de la cadena analogica. Una captura con 0
errores de CRC puede ser inservible igual.

    python proyecto/pc/revisar_captura.py resultados/P3_ref
"""
import argparse
import json
import sys
from pathlib import Path

import numpy as np

PROYECTO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROYECTO / 'pc'))
import trama as T  # noqa: E402

FS = float(T.SISTEMA['muestreo']['fs_hz'])
FONDO = float(1 << (T.SISTEMA['adc']['bits'] - 1))
NOTAS = {'Do': 261.63, 'Mi': 329.63, 'Sol': 392.0, 'Do5': 523.25}


def espectro(x):
    n = min(x.size, 131072)
    X = np.abs(np.fft.rfft((x[:n] - x[:n].mean())*np.hanning(n)))/n
    return np.fft.rfftfreq(n, 1/FS), X


def pico_en(f, X, f0, ancho=8.0):
    m = (f > f0 - ancho) & (f < f0 + ancho)
    return X[m].max() if m.any() else 0.0


def revisar(base):
    datos = np.load(base.with_suffix('.npz'))
    x = datos['muestras'].astype(float)
    continua, senal = x.mean(), x - x.mean()
    f, X = espectro(x)
    fondo = np.median(X[f > 2000])

    print(f'  {base.name}: {x.size} muestras = {x.size/FS:.2f} s')
    print(f'  nivel: senal {100*np.abs(senal).max()/FONDO:.0f} % · '
          f'rms {senal.std():.0f} · continua {continua:+.0f} de {FONDO:.0f}')

    print('\n  tonos de la melodia:')
    for nom, f0 in NOTAS.items():
        print(f'    {nom:4s} {f0:6.1f} Hz   {20*np.log10(pico_en(f, X, f0)/fondo):+5.1f} dB')

    print('\n  reparto de energia:')
    for lo, hi in ((0, 150), (150, 700), (700, 2000), (2000, 8000)):
        m = (f >= lo) & (f < hi)
        print(f'    {lo:5d}-{hi:5d} Hz  {100*np.sum(X[m]**2)/np.sum(X**2):5.1f} %')

    # Distorsion: no sirve medir armonicos, porque la melodia es una escala y sus
    # notas caen unas sobre los armonicos de otras (el Do agudo es el doble del Do).
    # Lo que si sirve: la senal limpia no tiene NADA arriba de 2 kHz, asi que todo lo
    # que aparezca ahi lo puso la cadena.
    arriba = np.sum(X[f >= 2000]**2)
    thd = 100*arriba/np.sum(X**2)
    print(f'\n  suciedad de la cadena: {thd:.0f} % de la energia arriba de 2 kHz')
    print(f'    la senal limpia no tiene nada ahi, asi que todo eso lo agrego el '
          f'amplificador, el parlante o el microfono')

    veredicto = []
    if 100*np.abs(senal).max()/FONDO < 15:
        veredicto.append('nivel bajo: acercar el microfono al parlante')
    if thd > 25:
        veredicto.append(f'cadena sucia ({thd:.0f} %): bajar el volumen del amplificador')
    elif thd > 10:
        veredicto.append(f'cadena algo sucia ({thd:.0f} %): se puede medir, pero mejor bajarla')
    if abs(continua) > 0.2*FONDO:
        veredicto.append('continua muy corrida: va a recortar de un lado')
    print('\n  ' + ('sirve para medir' if not veredicto else ' | '.join(veredicto)))
    return thd


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('captura', help='ruta sin extension, p. ej. resultados/P3_ref')
    a = ap.parse_args()
    base = PROYECTO / a.captura
    if not base.with_suffix('.npz').exists():
        raise SystemExit(f'no existe {base.with_suffix(".npz")}')
    revisar(base)


if __name__ == '__main__':
    main()

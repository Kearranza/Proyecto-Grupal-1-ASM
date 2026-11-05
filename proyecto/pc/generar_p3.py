"""Genera senales/p3.wav, la referencia limpia de la prueba P3 (Kevin).

P3 pide una senal arbitraria, no un multitono, asi que es un barrido de frecuencia
acotado a la banda util mas un armonico debil. Se reproduce desde la computadora
hacia la entrada del Nodo 1 y este mismo archivo es la referencia con la que
campana.py calcula las metricas.

    python proyecto/pc/generar_p3.py
"""
import json
import sys
from pathlib import Path

import numpy as np

PROYECTO = Path(__file__).resolve().parent.parent
RAIZ = PROYECTO.parent
sys.path.insert(0, str(RAIZ / 'src'))
import senales  # noqa: E402

COMUN = json.loads((RAIZ / 'metadatos' / 'configuraciones.json').read_text(encoding='utf-8'))['comun']
BANDA = json.loads((PROYECTO / 'config' / 'pruebas.json').read_text(
    encoding='utf-8'))['bandas']['banda_util_audio_hz']


def generar():
    fs, n = COMUN['fs_hz'], COMUN['muestras']
    t = np.arange(n)/fs
    f0, f1 = BANDA[0] + 50.0, BANDA[1] - 100.0
    tasa = (f1 - f0)/t[-1]
    barrido = np.sin(2*np.pi*(f0*t + 0.5*tasa*t**2))
    armonico = 0.25*np.sin(2*np.pi*(2*f0*t + 0.5*tasa*t**2) + 0.7)
    x = senales.desvanecer(barrido + armonico, fs, COMUN['fade_ms'])
    return COMUN['pico_maximo']*x/np.max(np.abs(x)), fs, f0, f1


def main():
    x, fs, f0, f1 = generar()
    destino = PROYECTO / 'senales'
    destino.mkdir(parents=True, exist_ok=True)
    senales.escribir_wav(destino / 'p3.wav', x, fs)
    leido, fs_leido = senales.leer_wav(destino / 'p3.wav')
    print(f'  senales/p3.wav: {leido.size} muestras a {fs_leido} Hz, '
          f'pico {np.max(np.abs(leido)):.3f}')
    print(f'  barrido de {f0:.0f} a {f1:.0f} Hz, dentro de la banda util {BANDA}')


if __name__ == '__main__':
    main()

"""WAV de P1 y P2 para la emisora de tablas (Kevin).

P1 y P2 salian del sketch de tonos del taller, que no tiene sonda: con tres placas no
se podian capturar. Aqui se generan como WAV, igual que P3-P5, para que la Pico comun
los toque desde flash y escuche el enlace a la vez.

    python proyecto/pc/wav_p1_p2.py
    python proyecto/pc/tabla_senal.py p1

Los tonos salen de config/pruebas.json (emisor.modos), los mismos del emisor del taller:
  P1: modo 1 (un tono) + la interferencia del modo 4, con su amplitud relativa
  P2: modo 3 (tres tonos) + ruido de banda arriba de la banda util, a 0 dB de SNR
Se usa src/senales.py de la Tarea 1, como en generar_wav_pruebas.py.
"""
import json
import sys
from pathlib import Path

import numpy as np

PROYECTO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROYECTO.parent / 'src'))
import senales  # noqa: E402

PRUEBAS = json.loads((PROYECTO / 'config' / 'pruebas.json').read_text(encoding='utf-8'))
SISTEMA = json.loads((PROYECTO / 'config' / 'sistema.json').read_text(encoding='utf-8'))
FS = SISTEMA['muestreo']['fs_hz']
# 2^17 muestras, como P3-P5: multiplo de todos los N permitidos
MUESTRAS = 1 << 17
# P2: empieza 1 kHz arriba de la banda de paso y termina 1 kHz antes de Nyquist
MARGEN_HZ = 1000.0


def tonos(modo):
    m = PRUEBAS['emisor']['modos'][modo]
    k = PRUEBAS['emisor']['tonos_utiles_del_modo'][modo]
    return m['frecuencias_hz'][:k], m['amplitudes'][:k]


def casos():
    f1, a1 = tonos('1')
    f3, a3 = tonos('3')
    m4 = PRUEBAS['emisor']['modos']['4']
    utiles4 = PRUEBAS['emisor']['tonos_utiles_del_modo']['4']
    f_int, a_int = m4['frecuencias_hz'][utiles4], m4['amplitudes'][utiles4]
    banda_alta = [PRUEBAS['bandas']['banda_util_audio_hz'][1] + MARGEN_HZ, FS/2 - MARGEN_HZ]
    return {
        'p1': {
            'id': 'p1', 'semilla': 3001,
            'descripcion': 'P1: un tono con una interferencia sinusoidal separada',
            'senal_util': 'multitono', 'frecuencias_utiles_hz': f1,
            'amplitudes_utiles': a1, 'fases_utiles_grados': [0]*len(f1),
            'tipo_ruido': 'tonal', 'frecuencia_interferencia_hz': f_int,
            'banda_ruido_hz': None,
            # La misma relacion que el modo 4 del emisor: tono de amplitud a_int contra 1
            'snr_objetivo_db': round(float(10*np.log10(sum(a*a for a in a1)/(a_int*a_int))), 4),
        },
        'p2': {
            'id': 'p2', 'semilla': 3002,
            'descripcion': 'P2: tres tonos con ruido de banda de alta frecuencia',
            'senal_util': 'multitono', 'frecuencias_utiles_hz': f3,
            'amplitudes_utiles': a3, 'fases_utiles_grados': [0, 40, 80][:len(f3)],
            'tipo_ruido': 'banda', 'banda_ruido_hz': banda_alta,
            'frecuencia_interferencia_hz': None, 'snr_objetivo_db': 0.0,
        },
    }


def main():
    comun, _ = senales.cargar_configuracion()
    comun = dict(comun, duracion_s=MUESTRAS/FS)
    if comun['fs_hz'] != FS:
        raise SystemExit(f"fs distinta: Tarea 1 = {comun['fs_hz']}, sistema.json = {FS}")
    todos = casos()
    for nombre in todos:
        datos = senales.generar_caso(nombre, comun, todos)
        d = senales.guardar_caso(datos, PROYECTO / 'senales' / nombre)
        ruido = d['frecuencia_interferencia_hz'] or d['banda_ruido_hz']
        print(f"  {nombre}: tonos {d['frecuencias_utiles_hz']} Hz, ruido {d['tipo_ruido']} {ruido}, "
              f"SNR {d['snr_obtenida_db']:.2f} dB")


if __name__ == '__main__':
    main()

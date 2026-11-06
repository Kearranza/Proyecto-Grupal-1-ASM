"""Senales de las pruebas P3-P5 para reproducir desde la computadora (Santiago).

El enunciado permite ruido "generado o reproducido desde un archivo". Se generan a
FS_HZ y 16 bits, con la SNR controlada, extendiendo src/senales.py de la Tarea 1
(no se crea otro generador):
  P3: audio o senal arbitraria + ruido de banda ancha
  P4: senal transitoria (p. ej. rafaga o escalon suavizado) + ruido impulsivo
  P5: senal con ruido de banda que se superpone parcialmente (como caso_3)
Ademas, al menos tres tipos de ruido de la seccion 2.2: tambien 60 Hz con armonicos.

    python proyecto/pc/generar_wav_pruebas.py

Guarda en proyecto/senales/ la limpia, el ruido y la contaminada de cada prueba,
porque la limpia es la referencia de las metricas.

Que se reproduce en la campana: contaminada.wav (lo que entra al ADC). limpia.wav
es la referencia de MSE y SNR; no se reproduce.

Decisiones (todas en PRUEBAS, mas abajo, para cambiarlas en un solo lugar):
  - Duracion de 8,192 s = 131 072 muestras = 2^17: multiplo exacto de todos los N
    permitidos (256 a 2048), asi la ultima ventana no queda incompleta.
  - P3 es una melodia corta con armonicos (suena a "audio" y sirve para la prueba
    auditiva); sus componentes llegan a 1,6 kHz, bajo Nyquist (8 kHz).
  - P4 son rafagas de tono que decaen, con clics al azar encima; las rafagas se
    ven en el tiempo, que es lo que P4 pide (respuesta temporal y latencia).
  - P5 usa los tonos de la Tarea 1 (440, 880, 1320 Hz) y una banda de ruido de
    1100 a 2000 Hz: solo pisa al de 1320 Hz, y el de 440 y el de 880 quedan libres.
    Asi el compromiso de P5 se puede medir tono por tono.
  - Semillas distintas por prueba, y distintas de las de la Tarea 1.
"""
import json
import sys
from pathlib import Path

import numpy as np

PROYECTO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROYECTO.parent / 'src'))
sys.path.insert(0, str(PROYECTO / 'pc'))
from pasos import ejecutar  
import senales  

CARPETA_SALIDA = PROYECTO / 'senales'
DURACION_S = 8.192
TOLERANCIA_SNR_DB = 1e-6

_TONOS = {'senal_util': 'multitono',
          'frecuencias_utiles_hz': [440, 880, 1320],
          'amplitudes_utiles': [1.0, 0.55, 0.3],
          'fases_utiles_grados': [0, 40, 80]}

PRUEBAS = {
    'p3': {
        'id': 'p3', 'semilla': 3003, 'snr_objetivo_db': 5.0,
        'descripcion': 'P3: melodia (audio) con ruido blanco de banda ancha',
        'senal_util': 'melodia',
        'notas': [[261.63, 0.2, 0.9], [329.63, 1.2, 0.9], [392.00, 2.2, 0.9],
                  [523.25, 3.2, 1.6], [440.00, 5.0, 0.9], [392.00, 6.0, 0.9],
                  [329.63, 7.0, 0.9]],
        'armonicos_melodia': [1, 2, 3], 'amplitudes_armonicos': [1.0, 0.4, 0.2],
        'tipo_ruido': 'blanco',
        'frecuencia_interferencia_hz': None, 'banda_ruido_hz': None,
    },
    'p4': {
        'id': 'p4', 'semilla': 3004, 'snr_objetivo_db': 6.0,
        'descripcion': 'P4: rafagas de tono (transitorios) con ruido impulsivo',
        'senal_util': 'rafagas',
        'eventos': [[880, 0.4, 80], [440, 1.4, 80], [1320, 2.4, 60], [660, 3.4, 80],
                    [880, 4.4, 80], [440, 5.4, 100], [1320, 6.4, 60], [880, 7.4, 80]],
        'tipo_ruido': 'impulsivo', 'tasa_impulsos_hz': 8.0, 'duracion_impulso_ms': 0.5,
        'frecuencia_interferencia_hz': None, 'banda_ruido_hz': None,
    },
    'p5': {
        'id': 'p5', 'semilla': 3005, 'snr_objetivo_db': 3.0,
        'descripcion': 'P5: tres tonos y ruido de banda que solo pisa al de 1320 Hz',
        **_TONOS,
        'tipo_ruido': 'banda', 'banda_ruido_hz': [1100, 2000],
        'frecuencia_interferencia_hz': None,
    },
    'zumbido_60hz': {
        'id': 'zumbido_60hz', 'semilla': 3060, 'snr_objetivo_db': 5.0,
        'descripcion': 'Extra (no es P1-P6): tres tonos con zumbido de 60 Hz y 4 armonicos',
        **_TONOS,
        'tipo_ruido': 'red', 'frecuencia_red_hz': 60.0,
        'armonicos_red': [1, 2, 3, 4, 5], 'amplitudes_red': [1.0, 0.5, 0.35, 0.2, 0.15],
        'frecuencia_interferencia_hz': None, 'banda_ruido_hz': None,
    },
}


def _parametros_comunes():
    # fs sale de config/sistema.json (fuente unica) y debe coincidir con la Tarea 1
    sistema = json.loads((PROYECTO / 'config' / 'sistema.json').read_text(encoding='utf-8'))
    comun, _ = senales.cargar_configuracion()
    comun = dict(comun)
    if comun['fs_hz'] != sistema['muestreo']['fs_hz']:
        raise ValueError(f"fs_hz distinta: Tarea 1 = {comun['fs_hz']}, "
                         f"sistema.json = {sistema['muestreo']['fs_hz']}")
    comun['duracion_s'] = DURACION_S
    return comun


def ruido_impulsivo():
    """Comprueba que el ruido impulsivo es de verdad impulsivo y no ruido blanco."""
    n = int(round(DURACION_S * 16000))
    generador = np.random.default_rng(1)
    impulsivo = senales.ruido_impulsivo(n, 16000, generador)
    blanco = senales.ruido_blanco(n, generador)

    def curtosis(x):                      # 3 para ruido gaussiano; alta si hay picos
        x = x - x.mean()
        return float(np.mean(x**4) / np.mean(x**2)**2)

    k_imp, k_bl = curtosis(impulsivo), curtosis(blanco)
    if not (k_bl < 3.5 and k_imp > 10):
        raise AssertionError(f'curtosis impulsivo = {k_imp:.1f} (se esperaba > 10), '
                             f'blanco = {k_bl:.2f} (se esperaba ~3)')


def interferencia_60hz():
    """Comprueba que el zumbido tiene sus picos en 60 Hz y los armonicos, y solo ahi."""
    fs, n = 16000, int(round(DURACION_S * 16000))
    t = np.arange(n) / fs
    x = senales.interferencia_red(t, np.random.default_rng(1))
    espectro = np.abs(np.fft.rfft(x * np.hanning(n)))
    f = np.fft.rfftfreq(n, 1/fs)
    esperados = [60.0 * k for k in range(1, 6)]
    for fe in esperados:
        cerca = np.abs(f - fe) < 2.0
        if f[cerca][np.argmax(espectro[cerca])] < fe - 1.0 or \
           f[cerca][np.argmax(espectro[cerca])] > fe + 1.0:
            raise AssertionError(f'no hay pico en {fe:.0f} Hz')
    fuera = np.ones_like(f, bool)
    for fe in esperados:
        fuera &= np.abs(f - fe) > 5.0
    if espectro[fuera].max() > 0.01 * espectro.max():
        raise AssertionError('hay energia fuera de los armonicos de 60 Hz')


def generar(prueba):
    comun = _parametros_comunes()
    caso = PRUEBAS[prueba]
    datos = senales.generar_caso(prueba, comun, {prueba: caso})
    descripcion = senales.guardar_caso(datos, CARPETA_SALIDA / prueba)
    error = abs(descripcion['snr_obtenida_db'] - caso['snr_objetivo_db'])
    if error > TOLERANCIA_SNR_DB:
        raise AssertionError(f"SNR obtenida {descripcion['snr_obtenida_db']:.6f} dB, "
                             f"objetivo {caso['snr_objetivo_db']} dB")
    print(f"      {prueba:13s} {descripcion['tipo_ruido']:9s} "
          f"SNR {descripcion['snr_obtenida_db']:.4f} dB "
          f"(tras cuantizar a 16 bits {descripcion['snr_tras_cuantizar_db']:.4f} dB) "
          f"| {descripcion['duracion_s']:.3f} s | ganancia {descripcion['ganancia_comun']:.3f}")


def main():
    ejecutar('Senales de prueba', [
        ('ruido impulsivo', ruido_impulsivo),
        ('60 Hz con armonicos', interferencia_60hz),
        ('P3', lambda: generar('p3')),
        ('P4', lambda: generar('p4')),
        ('P5', lambda: generar('p5')),
        ('extra: zumbido de 60 Hz', lambda: generar('zumbido_60hz')),
    ])


if __name__ == '__main__':
    main()
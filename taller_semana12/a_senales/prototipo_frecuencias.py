"""BLOQUE A - Senales y frecuencias del taller (rama: taller-senales)

Responde la pregunta "en que frecuencias trabaja el sistema y por que" (punto 1
del taller). Todo lo demas se apoya en esto:
  - B usa senal_del_modo() como entrada del sistema completo.
  - C compara lo que mide a la salida de la Pico contra senal_del_modo().
  - D usa senal_del_modo() como referencia de la validacion del receptor.

Correr desde la raiz del repo:
    python taller_semana12/a_senales/prototipo_frecuencias.py

Entrega de A en el PDF (informe/secciones/1_senales.tex):
  1. Justificacion de fs y N: Nyquist, resolucion fs/N, duracion del bloque,
     y por que sirve para el ADC de la Pico W y el PWM de la Pico.
  2. Tabla de los modos del emisor con su bin k = f/(fs/N) y si cae o no en un
     bin exacto (fuga espectral -> por que se usa ventana Hann).
  3. Espectro de cada modo, incluyendo el efecto de cuantizar a 8 bits (PWM).
  4. DFT por definicion vs FFT: ya esta en src/dft_fft.py y pruebas/test_fft.py
     (Tarea 1). Aqui solo falta la tabla de tiempos con N del taller.
"""
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comun import parametros as P  # noqa: E402
from comun.pasos import ejecutar  # noqa: E402

P.usar_src()
import senales  # noqa: E402

# La amplitud del JSON tiene 4 decimales; ese redondeo mueve la SNR menos de 0.001 dB
TOLERANCIA_SNR_DB = 0.01


def resumen_muestreo(p):
    # Hecho como ejemplo del estilo: todo sale de p, nada escrito a mano
    fs, n = p['fs_hz'], p['n_bloque']
    df = fs / n
    print(f'    fs = {fs} Hz, Nyquist = {fs/2:.0f} Hz, N = {n}, '
          f'resolucion = {df:.3f} Hz, bloque = {1000*n/fs:.1f} ms')
    for m in p['emisor']['modos']:
        for f in m['frecuencias_hz']:
            k = f / df
            exacto = 'si' if abs(k - round(k)) < 1e-9 else 'no'
            print(f'    modo {m["id"]}: {f:6.0f} Hz -> bin {k:7.2f} (exacto: {exacto})')
            if f >= fs / 2:
                raise ValueError(f'{f} Hz viola Nyquist con fs = {fs} Hz')


def senal_del_modo(p, identificador, n_muestras=None, cuantizar_salida=True):
    """Senal que debe salir del emisor en el modo dado, normalizada a [-1, 1].

    Debe coincidir muestra a muestra con lo que calcula el emisor:
      - suma de senos con las amplitudes del modo,
      - escalada para que el pico sea p['emisor']['nivel_pico'],
      - si cuantizar_salida, redondeada a p['emisor']['bits_salida'] bits.
    Devuelve (t, x).
    """
    m = P.modo(p, identificador)
    emisor = p['emisor']
    n = p['n_bloque'] if n_muestras is None else n_muestras
    t = np.arange(n) / emisor['fs_hz']
    # Fase 0 en todas las componentes: el acumulador de fase del DDS arranca en 0
    x = senales.multitono(t, m['frecuencias_hz'], m['amplitudes'], [0] * len(m['amplitudes']))
    # Misma escala que el emisor: si todos los senos coinciden en su pico, la suma vale nivel_pico
    x = x * emisor['nivel_pico'] / sum(m['amplitudes'])
    if cuantizar_salida:
        x = cuantizar(x, emisor['bits_salida'])
    return t, x


def cuantizar(x, bits):
    """Lo que hace el PWM: nivel entero de 0 a 2^bits - 1, con el cero en la mitad."""
    mitad = 2 ** (bits - 1)
    nivel = np.clip(np.round(x * mitad) + mitad, 0, 2 ** bits - 1)
    return (nivel - mitad) / mitad


def verificar_snr_modo_3(p):
    """Confirmar que la amplitud de 3500 Hz del modo 3 da la misma SNR que caso_1."""
    caso = p['casos']['caso_1']
    m = P.modo(p, 3)
    # Senos de frecuencias distintas son ortogonales: la potencia total es la suma de A^2/2
    potencias = {f: a ** 2 / 2 for f, a in zip(m['frecuencias_hz'], m['amplitudes'])}
    p_ruido = potencias.pop(caso['frecuencia_interferencia_hz'])
    p_util = sum(potencias.values())
    snr = 10 * np.log10(p_util / p_ruido)
    a_exacta = np.sqrt(2 * p_util / 10 ** (caso['snr_objetivo_db'] / 10))
    print(f'    potencia util = {p_util:.5f}, interferencia = {p_ruido:.5f}')
    print(f'    SNR = {snr:.4f} dB (objetivo {caso["snr_objetivo_db"]} dB), '
          f'amplitud exacta = {a_exacta:.6f}')
    if abs(snr - caso['snr_objetivo_db']) > TOLERANCIA_SNR_DB:
        raise ValueError(f'La amplitud del modo 3 da {snr:.4f} dB, no {caso["snr_objetivo_db"]} dB')
    return snr


def figura_espectros(p):
    """Espectro (dB) de cada modo con y sin cuantizacion a 8 bits.

    Guardar en P.CARPETA_FIGURAS / 'a_espectros_modos.png'.
    Marcar las frecuencias esperadas y el piso de cuantizacion (~6.02*bits + 1.76 dB).
    """
    raise NotImplementedError('TODO (A): figura de espectros de los modos')


def tabla_dft_vs_fft(p):
    """Tiempo de dft() vs fft() de src/dft_fft.py para N = p['n_bloque'].

    Reportar tiempo promedio de varias corridas y el error maximo entre ambas.
    """
    raise NotImplementedError('TODO (A): tabla de tiempos DFT vs FFT')


def main():
    p = P.cargar()
    ejecutar('Bloque A - senales y frecuencias', [
        ('resumen de muestreo', lambda: resumen_muestreo(p)),
        ('senal de cada modo', lambda: [senal_del_modo(p, m['id']) for m in p['emisor']['modos']]),
        ('SNR del modo 3', lambda: verificar_snr_modo_3(p)),
        ('figura de espectros', lambda: figura_espectros(p)),
        ('DFT vs FFT', lambda: tabla_dft_vs_fft(p)),
    ])


if __name__ == '__main__':
    main()

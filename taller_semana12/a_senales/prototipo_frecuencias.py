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
import time
from pathlib import Path

import numpy as np
from matplotlib.figure import Figure

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comun import parametros as P  # noqa: E402
from comun.pasos import ejecutar  # noqa: E402

P.usar_src()
import dft_fft  # noqa: E402
import graficas as G  # noqa: E402
import metricas  # noqa: E402
import senales  # noqa: E402

# La amplitud del JSON tiene 4 decimales; ese redondeo mueve la SNR menos de 0.001 dB
TOLERANCIA_SNR_DB = 0.01
# La DFT por definicion tarda segundos con N = 1024; la FFT, milisegundos
REPETICIONES_DFT = 3
REPETICIONES_FFT = 50


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
    fs, bits = p['emisor']['fs_hz'], p['emisor']['bits_salida']
    piso = piso_cuantizacion_db(bits, p['n_bloque'])
    modos = p['emisor']['modos']
    fig = Figure(figsize=(11.5, 2.8 * len(modos)), facecolor=G.SUPERFICIE)
    for i, m in enumerate(modos):
        ax = fig.add_subplot(len(modos), 1, i + 1)
        for cuantizada, color, nombre in ((False, G.LIMPIA, 'Ideal'),
                                          (True, G.CONTAMINADA, f'Cuantizada a {bits} bits')):
            f, mag = espectro_dbfs(senal_del_modo(p, m['id'], cuantizar_salida=cuantizada)[1], fs)
            ax.plot(f, mag, color=color, linewidth=1.0, label=nombre)
        for frecuencia in m['frecuencias_hz']:
            ax.axvline(frecuencia, color=G.APAGADO, linewidth=0.8, linestyle=':')
        ax.axhline(piso, color=G.TINTA_SUAVE, linewidth=0.8, linestyle='--',
                   label=f'Piso de cuantización por bin ({piso:.1f} dB)')
        ax.set(xlabel='Frecuencia (Hz)', ylabel='Magnitud (dB)', xlim=(0, fs / 2),
               ylim=(piso - 40, 5), title=f'Modo {m["id"]}: {m["nombre"]}')
        ax.legend(frameon=False, fontsize=8, labelcolor=G.TINTA_SUAVE, loc='upper right')
        G._estilo(ax)
    fig.tight_layout()
    ruta = P.CARPETA_FIGURAS / 'a_espectros_modos.png'
    fig.savefig(ruta, dpi=160, facecolor=G.SUPERFICIE)
    print(f'    piso de cuantizacion = {piso:.1f} dB por bin, figura en {ruta.name}')


def espectro_dbfs(x, fs):
    # Se le resta la ganancia de la ventana para que un seno de amplitud A salga en 20*log10(A)
    f, magnitud_db, _ = metricas.espectro(x, fs)
    return f, magnitud_db - 20 * np.log10(np.sum(np.hanning(x.size)) / 2)


def piso_cuantizacion_db(bits, n):
    # Ruido de cuantizacion: varianza delta^2/12 repartida en los N bins y vista por la ventana Hann
    delta = 1 / 2 ** (bits - 1)
    w = np.hanning(n)
    return 10 * np.log10(4 * delta ** 2 / 12 * np.sum(w ** 2) / np.sum(w) ** 2)


def tabla_dft_vs_fft(p):
    """Tiempo de dft() vs fft() de src/dft_fft.py para N = p['n_bloque'].

    Reportar tiempo promedio de varias corridas y el error maximo entre ambas.
    """
    n = p['n_bloque']
    x = senal_del_modo(p, 3)[1]
    t_dft, X_dft = tiempo_promedio(dft_fft.dft, x, REPETICIONES_DFT)
    t_fft, X_fft = tiempo_promedio(dft_fft.fft, x, REPETICIONES_FFT)
    # Multiplicaciones complejas: N^2 en la DFT, (N/2)*log2(N) en la FFT radix-2
    fila = {'n': n, 't_dft_ms': 1000 * t_dft, 't_fft_ms': 1000 * t_fft,
            'razon_medida': t_dft / t_fft, 'razon_teorica': n ** 2 / (n / 2 * np.log2(n)),
            'error_max': np.max(np.abs(X_dft - X_fft)), 'bin_maximo': np.max(np.abs(X_fft))}
    print(f'    N = {n}: DFT {fila["t_dft_ms"]:.1f} ms, FFT {fila["t_fft_ms"]:.2f} ms, '
          f'medida {fila["razon_medida"]:.0f}x, teorica {fila["razon_teorica"]:.0f}x, '
          f'error max {fila["error_max"]:.2e}')
    ruta = P.CARPETA_RESULTADOS / 'a_dft_vs_fft.csv'
    ruta.write_text(','.join(fila) + '\n' + ','.join(f'{v:.6g}' for v in fila.values()) + '\n',
                    encoding='utf-8')
    escribir_cifras_tex(fila)
    return fila


def escribir_cifras_tex(fila):
    # El informe toma las cifras de aqui, asi el PDF siempre coincide con la ultima corrida
    def coma(valor, decimales):
        return f'{valor:.{decimales}f}'.replace('.', '{,}')

    def cientifico(valor):
        mantisa, exponente = f'{valor:.1e}'.split('e')
        return f'{mantisa.replace(".", "{,}")}\\times10^{{{int(exponente)}}}'

    macros = {
        'aRepDft': str(REPETICIONES_DFT),
        'aRepFft': str(REPETICIONES_FFT),
        'aDftMs': coma(fila['t_dft_ms'], 0),
        'aFftMs': coma(fila['t_fft_ms'], 2),
        'aRazonMedida': coma(fila['razon_medida'], 0),
        'aRazonTeorica': coma(fila['razon_teorica'], 0),
        'aErrorMax': cientifico(fila['error_max']),
        'aBinMaximo': coma(fila['bin_maximo'], 0),
        'aErrorRelativo': cientifico(fila['error_max'] / fila['bin_maximo']),
    }
    lineas = [f'\\def\\{nombre}{{{valor}}}' for nombre, valor in macros.items()]
    ruta = P.CARPETA_RESULTADOS / 'a_dft_vs_fft.tex'
    ruta.write_text('% Generado por a_senales/prototipo_frecuencias.py. No editar a mano.\n'
                    + '\n'.join(lineas) + '\n', encoding='utf-8')


def tiempo_promedio(funcion, x, repeticiones):
    inicio = time.perf_counter()
    for _ in range(repeticiones):
        resultado = funcion(x)
    return (time.perf_counter() - inicio) / repeticiones, resultado


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

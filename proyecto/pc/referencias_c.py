"""Genera los datos de referencia con los que se validan los modulos en C.

Requisito 5.5 del proyecto: lo que corre en el micro se valida contra la
implementacion de referencia. Aqui la referencia es el codigo de la Tarea 1
(src/), y los modulos de C se comparan contra estos archivos en pruebas_c/.

    python proyecto/pc/referencias_c.py

Escribe en proyecto/pruebas_c/datos/:
    iir.txt          sos eliptico de rechazo de banda + entrada + salida de scipy
    metricas.txt     s, x, y + MSE y SNR calculados con src/metricas.py
    diagnostico_caso_N.txt   espectro de un bloque + tipo y frecuencia esperados
"""
import sys
from pathlib import Path

import numpy as np
from scipy import signal

PROYECTO = Path(__file__).resolve().parent.parent
RAIZ = PROYECTO.parent
sys.path.insert(0, str(RAIZ / 'src'))
import filtrado_espectral  # noqa: E402
import metricas  # noqa: E402
import senales  # noqa: E402

sys.path.insert(0, str(PROYECTO / 'pc'))
from generar_config_h import cargar as cargar_sistema  # noqa: E402

DATOS = PROYECTO / 'pruebas_c' / 'datos'
TIPOS = {'tonal': 1, 'blanco': 2, 'banda': 3}
MUESTRAS_IIR = 4096
MUESTRAS_METRICAS = 8192
INICIO_BLOQUE = 4096   # lejos del desvanecimiento inicial de los WAV


def escribir(ruta, encabezado, filas):
    with open(ruta, 'w', encoding='utf-8') as f:
        f.write(encabezado + '\n')
        for fila in filas:
            f.write(' '.join(f'{v:.9g}' for v in np.atleast_1d(fila)) + '\n')
    print(f'  {ruta.relative_to(PROYECTO)}')


def referencia_iir(fs):
    # Rechazo de banda eliptico alrededor de la interferencia del caso_1, como en la Tarea 1
    sos = signal.iirdesign([3300, 3700], [3450, 3550], 1.0, 40.0,
                           ftype='ellip', output='sos', fs=fs)
    x = senales.cargar_caso('caso_1')['contaminada'][:MUESTRAS_IIR]
    y = signal.sosfilt(sos, x)
    with open(DATOS / 'iir.txt', 'w', encoding='utf-8') as f:
        f.write(f'{len(sos)}\n')
        for etapa in sos:
            f.write(' '.join(f'{v:.12g}' for v in etapa) + '\n')
        f.write(f'{len(x)}\n')
        for a, b in zip(x, y):
            f.write(f'{a:.9g} {b:.9g}\n')
    print(f'  {(DATOS / "iir.txt").relative_to(PROYECTO)} ({len(sos)} etapas)')


def referencia_metricas():
    caso = senales.cargar_caso('caso_1')
    s = caso['limpia'][:MUESTRAS_METRICAS]
    x = caso['contaminada'][:MUESTRAS_METRICAS]
    y = s + 0.01 * np.random.default_rng(5).standard_normal(s.size)
    esperado = [metricas.mse(s, y), metricas.snr_db(s, x), metricas.snr_db(s, y),
                metricas.delta_snr_db(s, x, y)]
    escribir(DATOS / 'metricas.txt', f'{s.size}', list(np.column_stack([s, x, y])) + [esperado])


def referencia_diagnostico(fs, n):
    # Lo esperado es la VERDAD de cada caso (metadatos/configuraciones.json), no lo que
    # dice clasificar_ruido() de la Tarea 1: ese clasificador marca caso_3 como tonal y
    # toma el tono util de 440 Hz como interferencia. El nodo 1 tiene que mejorarlo.
    for caso_id in ('caso_1', 'caso_2', 'caso_3'):
        caso = senales.cargar_caso(caso_id)
        x = caso['contaminada'][INICIO_BLOQUE:INICIO_BLOQUE + n]
        magnitud = np.abs(np.fft.fft(x * np.hanning(n)))
        config = caso['config']
        tipo = config['tipo_ruido']
        if tipo == 'tonal':
            frecuencia = config['frecuencia_interferencia_hz']
        elif tipo == 'banda':
            frecuencia = float(np.mean(config['banda_ruido_hz']))
        else:
            frecuencia = 0.0
        tarea1 = filtrado_espectral.clasificar_ruido(magnitud)
        aviso = '' if tarea1 == tipo else f'  <- la Tarea 1 decia "{tarea1}"'
        escribir(DATOS / f'diagnostico_{caso_id}.txt',
                 f'{n} {fs} {TIPOS[tipo]} {frecuencia:.3f}', magnitud[:n // 2 + 1])
        print(f'    esperado: {tipo}, {frecuencia:.0f} Hz{aviso}')


def main():
    sistema = cargar_sistema()
    fs, n = sistema['muestreo']['fs_hz'], sistema['bloque']['n_por_defecto']
    DATOS.mkdir(parents=True, exist_ok=True)
    referencia_iir(fs)
    referencia_metricas()
    referencia_diagnostico(fs, n)


if __name__ == '__main__':
    main()

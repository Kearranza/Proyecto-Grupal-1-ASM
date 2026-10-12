# Comparacion de los metodos de filtrado contra la senal limpia (punto 2.5)

import csv
import json
from pathlib import Path

import numpy as np

import metricas
import senales

RAIZ = senales.RAIZ
CARPETA_SALIDAS = RAIZ / 'resultados' / 'salidas'
ARCHIVO_CSV = RAIZ / 'resultados' / 'metricas.csv'
ARCHIVO_TXT = RAIZ / 'resultados' / 'metricas.txt'

METODOS = {'temporal': 'Filtro FIR/IIR (2.3)', 'espectral': 'FFT/IFFT (2.4)'}
ANCHO_TONO_HZ = 60.0
RETARDO_MAXIMO = 2000


def banda_dominante(caso, fs):
    # La banda donde vive el ruido de cada caso, para medir cuanto se atenuo
    if caso['tipo_ruido'] == 'tonal':
        centro = caso['frecuencia_interferencia_hz']
        return (centro - 50.0, centro + 50.0)
    if caso['tipo_ruido'] == 'banda':
        return tuple(float(v) for v in caso['banda_ruido_hz'])
    # El ruido blanco no tiene banda propia: se mide fuera de la banda util
    return (max(caso['frecuencias_utiles_hz']) + 200.0, fs/2)


def alinear(referencia, estimada):
    # Un filtro introduce retardo; se estima por correlacion y se compensa antes
    # de medir, porque si no el MSE castiga el retardo y no la distorsion
    n = min(referencia.size, estimada.size)
    referencia, estimada = referencia[:n], estimada[:n]
    tamano = 1
    while tamano < 2*n:
        tamano <<= 1
    correlacion = np.fft.irfft(np.fft.rfft(estimada, tamano) *
                               np.conj(np.fft.rfft(referencia, tamano)), tamano)
    busqueda = np.concatenate([correlacion[:RETARDO_MAXIMO + 1],
                               correlacion[-RETARDO_MAXIMO:]])
    indice = int(np.argmax(np.abs(busqueda)))
    retardo = indice if indice <= RETARDO_MAXIMO else indice - busqueda.size
    if retardo > 0:
        return referencia[:n - retardo], estimada[retardo:n], retardo
    if retardo < 0:
        return referencia[-retardo:n], estimada[:n + retardo], retardo
    return referencia, estimada, 0


def extras_de(ruta_wav):
    # Cada bloque puede dejar un JSON al lado con su tiempo y su memoria
    ruta = ruta_wav.with_suffix('.json')
    if not ruta.exists():
        return {}
    return json.loads(ruta.read_text(encoding='utf-8'))


def medir(datos, estimada, extras=None):
    limpia, alineada, retardo = alinear(datos['limpia'], estimada)
    contaminada_recortada = datos['contaminada'][:limpia.size]
    fs = datos['fs']
    utiles = metricas.bandas_utiles_de(datos['config']['frecuencias_utiles_hz'],
                                       ANCHO_TONO_HZ)
    banda = banda_dominante(datos['config'], fs)
    extras = extras or {}
    return {
        'caso': datos['id'],
        'mse': metricas.mse(limpia, alineada),
        'snr_db': metricas.snr_db(limpia, alineada),
        'delta_snr_db': metricas.snr_db(limpia, alineada) -
                        metricas.snr_db(limpia, contaminada_recortada),
        'energia_util_conservada': metricas.energia_util_conservada(limpia, alineada, fs, utiles),
        'atenuacion_db': metricas.atenuacion_db(contaminada_recortada, alineada, fs, banda),
        'retardo_muestras': retardo,
        'tiempo_por_bloque_ms': extras.get('tiempo_por_bloque_ms', ''),
        'memoria_bytes': extras.get('memoria_bytes', ''),
    }


def fila_sin_filtrar(datos):
    # La linea de base: que da la senal contaminada sin tocarla
    fila = medir(datos, datos['contaminada'])
    fila['metodo'] = 'sin filtrar'
    return fila


def recolectar():
    _, casos = senales.cargar_configuracion()
    filas, faltantes = [], []
    for identificador in casos:
        datos = senales.cargar_caso(identificador)
        filas.append(fila_sin_filtrar(datos))
        for metodo in METODOS:
            ruta = CARPETA_SALIDAS / f'{identificador}_{metodo}.wav'
            if not ruta.exists():
                faltantes.append(ruta.name)
                continue
            estimada, fs = senales.leer_wav(ruta)
            if fs != datos['fs']:
                raise ValueError(f'{ruta.name} esta a {fs} Hz y deberia estar a {datos["fs"]} Hz')
            fila = medir(datos, estimada, extras_de(ruta))
            fila['metodo'] = metodo
            filas.append(fila)
    return filas, faltantes


COLUMNAS = ['caso', 'metodo', 'mse', 'snr_db', 'delta_snr_db',
            'energia_util_conservada', 'atenuacion_db', 'retardo_muestras',
            'tiempo_por_bloque_ms', 'memoria_bytes']


def _redondear(fila):
    salida = dict(fila)
    salida['mse'] = f"{fila['mse']:.3e}"
    for clave in ('snr_db', 'delta_snr_db', 'atenuacion_db'):
        salida[clave] = f'{fila[clave]:.2f}'
    salida['energia_util_conservada'] = f"{fila['energia_util_conservada']:.3f}"
    return salida


def escribir(filas):
    ARCHIVO_CSV.parent.mkdir(parents=True, exist_ok=True)
    with open(ARCHIVO_CSV, 'w', newline='', encoding='utf-8') as archivo:
        escritor = csv.DictWriter(archivo, fieldnames=COLUMNAS)
        escritor.writeheader()
        for fila in filas:
            escritor.writerow(_redondear({k: fila[k] for k in COLUMNAS}))

    encabezados = ['caso', 'metodo', 'MSE', 'SNR dB', 'dSNR dB', 'E util',
                   'aten dB', 'retardo', 'ms/bloque', 'bytes']
    anchos = [max(len(e), 11) for e in encabezados]
    lineas = ['  '.join(e.rjust(a) for e, a in zip(encabezados, anchos)),
              '-'*(sum(anchos) + 2*(len(anchos) - 1))]
    for fila in filas:
        redondeada = _redondear(fila)
        lineas.append('  '.join(str(redondeada[c]).rjust(a)
                                for c, a in zip(COLUMNAS, anchos)))
    ARCHIVO_TXT.write_text('\n'.join(lineas) + '\n', encoding='utf-8')
    return '\n'.join(lineas)


def main():
    filas, faltantes = recolectar()
    print(escribir(filas))
    if faltantes:
        print('\nTodavia faltan estas salidas:')
        for nombre in faltantes:
            print('  ', nombre)
    else:
        print('\nEstan las seis salidas, la tabla del 2.5 esta completa.')
    return filas


if __name__ == '__main__':
    main()

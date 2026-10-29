import csv
import json
import sys
from pathlib import Path
from time import perf_counter

import numpy as np
from scipy import signal

TALLER = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TALLER))
from comun import parametros as P

P.usar_src()
import filtros_temporales as temporal
import filtrado_espectral as espectral
import metricas
from dft_fft import fft, ifft


def cargar():
    p = P.cargar()
    p['b'] = json.loads(Path(__file__).with_name('configuracion_b.json').read_text())
    p['n_muestras'] = json.loads(P.ARCHIVO_TAREA1.read_text())['comun']['muestras']
    n, fs = p['n_bloque'], p['fs_hz']
    if n < 4 or n & (n - 1) or fs <= 0:
        raise ValueError('La FFT requiere N potencia de dos y fs positiva')
    if p['ventana'] != 'hann' or not 0 < p['solapamiento'] < 1:
        raise ValueError('Se requiere Hann con solapamiento entre cero y uno')
    if p['emisor']['fs_hz'] != fs:
        raise ValueError('Este modelo requiere igual fs en emisor y receptor')
    return p


def frecuencias_utiles(p):
    return p['casos']['caso_1']['frecuencias_utiles_hz']


def entrada(p, identificador):
    """Genera una referencia provisional sin cuantizacion y con escala comun."""
    # TODO (A): sustituir este generador por senal_del_modo(...,
    # cuantizar_salida=False), acordando fases, longitud y escala de la referencia.
    modo = P.modo(p, identificador)
    t = np.arange(p['n_muestras']) / p['fs_hz']
    limpia, mezcla = np.zeros_like(t), np.zeros_like(t)
    for f, a in zip(modo['frecuencias_hz'], modo['amplitudes'], strict=True):
        if not 0 < f < p['fs_hz'] / 2:
            raise ValueError('Frecuencia fuera de la banda de Nyquist')
        componente = a * np.sin(2 * np.pi * f * t)
        mezcla += componente
        if f in frecuencias_utiles(p):
            limpia += componente
    escala = p['emisor']['nivel_pico'] / np.max(np.abs(mezcla))
    return limpia * escala, mezcla * escala


def cadena_fisica(x, p):
    """Modelo promedio PWM, RC opcional y ADC; entrega cuentas, voltios y senal."""
    x = np.asarray(x, dtype=float)
    vref = p['receptor']['vref_v']
    niveles_pwm = 2 ** p['emisor']['bits_salida'] - 1
    niveles_adc = 2 ** p['receptor']['bits_adc'] - 1
    duty = (x + 1) / 2
    pwm = np.rint(np.clip(duty, 0, 1) * niveles_pwm)
    voltios = pwm / niveles_pwm * vref
    fc = p['emisor']['filtro_reconstruccion']['fc_hz']
    # TODO (C): confirmar fc y topologia reales; sin fc se omite el RC.
    # Dos polos aislados no modelan la carga mutua de dos etapas RC pasivas.
    if fc is not None:
        if not 0 < fc < p['fs_hz'] / 2:
            raise ValueError('fc debe estar entre cero y Nyquist')
        polo = np.exp(-2 * np.pi * fc / p['fs_hz'])
        for _ in range(2):
            voltios = signal.lfilter([1 - polo], [1, -polo], voltios,
                                     zi=[polo * vref / 2])[0]
    adc = np.rint(np.clip(voltios / vref, 0, 1) * niveles_adc)
    adquirida = 2 * adc / niveles_adc - 1
    return {
        'senal': adquirida, 'pwm': pwm, 'adc': adc, 'voltios': voltios,
        'recortes_pwm': int(np.count_nonzero((duty < 0) | (duty > 1))),
        'recortes_adc': int(np.count_nonzero((voltios < 0) | (voltios > vref))),
        'rc_activo': fc is not None,
    }


def nodo_1_adquisicion(x, p):
    n, fs = p['n_bloque'], p['fs_hz']
    salto = int(n * (1 - p['solapamiento']))
    if salto < 1:
        raise ValueError('El avance de bloque debe ser positivo')
    centrada = np.asarray(x) - np.mean(x)
    extendida = np.pad(centrada, (0, max(0, n - len(centrada))))
    magnitudes = [np.abs(fft(extendida[i:i+n] * np.hanning(n)))[:n//2+1]
                  for i in range(0, len(extendida)-n+1, salto)]
    magnitud = np.mean(magnitudes, axis=0)
    candidata = magnitud.copy()
    f = np.fft.rfftfreq(n, 1 / fs)
    ancho = p['b']['proteccion_bins'] * fs / n
    candidata[f <= ancho] = 0
    for util in frecuencias_utiles(p):
        candidata[np.abs(f - util) <= ancho] = 0
    k = int(np.argmax(candidata))
    razon = float(20 * np.log10(max(candidata[k], 1e-20) /
                               max(np.max(magnitud), 1e-20)))
    detectada = bool(razon > p['b']['umbral_interferencia_db'])
    # TODO (D): acordar el formato de diagnostico y los margenes de aceptacion.
    return {'tipo': 'tonal' if detectada else 'sin_interferencia',
            'frecuencia_hz': float(f[k]) if detectada else None,
            'nivel_relativo_db': razon, 'dc': float(np.mean(x))}


def disenar_temporal(diagnostico, p):
    """Un notch de segundo orden evita alterar las bandas utiles alejadas."""
    if diagnostico['tipo'] == 'sin_interferencia':
        return None
    b, a = signal.iirnotch(diagnostico['frecuencia_hz'],
                          p['b']['q_notch'], fs=p['fs_hz'])
    sos = signal.tf2sos(b, a)
    if not temporal.es_estable(sos)[0]:
        raise ValueError('El filtro temporal no es estable')
    return sos


def nodo_2_temporal(x, diagnostico, p):
    """Procesa la captura completa para conservar los estados entre muestras."""
    x = np.asarray(x) - diagnostico['dc']
    sos = disenar_temporal(diagnostico, p)
    return x.copy() if sos is None else temporal.filtrar_sos_manual(sos, x)


def nodo_3_espectral(x, diagnostico, p):
    """FFT propia y reconstruccion con doble Hann, relleno y normalizacion."""
    n, fs = p['n_bloque'], p['fs_hz']
    salto = int(n * (1 - p['solapamiento']))
    centrada = np.asarray(x) - diagnostico['dc']
    rellena = np.pad(centrada, (n, n))
    rellena = np.pad(rellena, (0, (-(len(rellena)-n)) % salto))
    salida, pesos = np.zeros_like(rellena), np.zeros_like(rellena)
    ventana = np.hanning(n)
    ganancia = np.ones(n)
    if diagnostico['tipo'] == 'tonal':
        f = np.fft.fftfreq(n, 1 / fs)
        banda = np.abs(np.abs(f) - diagnostico['frecuencia_hz']) <= (
            p['b']['rechazo_semi_ancho_bins'] * fs / n)
        ganancia[banda] = p['b']['ganancia_espectral']
    # Se reutilizan las primitivas; reducir_ruido reclasifica y fija tonos internamente.
    for i in range(0, len(rellena)-n+1, salto):
        X = fft(rellena[i:i+n] * ventana)
        Y = espectral.conservar_simetria(X * ganancia)
        salida[i:i+n] += ifft(Y).real * ventana
        pesos[i:i+n] += ventana ** 2
    validos = pesos > np.finfo(float).eps
    salida[validos] /= pesos[validos]
    return salida[n:n+len(x)]


def evaluar(limpia, contaminada, salidas, p, diagnostico):
    """Evalua un intervalo comun sin corregir fase ni ganancia """
    margen = p['b']['recorte_evaluacion_bloques'] * p['n_bloque']
    if len(limpia) <= 2 * margen:
        raise ValueError('Captura demasiado corta para el intervalo de evaluacion')
    tramo = slice(margen, -margen if margen else None)
    ref, entrada_adc = limpia[tramo], contaminada[tramo]
    ancho = p['b']['proteccion_bins'] * p['fs_hz'] / p['n_bloque']
    bandas = metricas.bandas_utiles_de(frecuencias_utiles(p), ancho)
    filas = []
    for nombre, salida in salidas.items():
        y = salida[tramo]
        atenuacion = None
        if diagnostico['frecuencia_hz'] is not None:
            f = diagnostico['frecuencia_hz']
            atenuacion = metricas.atenuacion_db(entrada_adc, y, p['fs_hz'],
                                               (f-ancho, f+ancho))
        filas.append({'metodo': nombre, 'mse': metricas.mse(ref, y),
                      'snr_entrada_db': metricas.snr_db(ref, entrada_adc),
                      'snr_salida_db': metricas.snr_db(ref, y),
                      'delta_snr_db': metricas.delta_snr_db(ref, entrada_adc, y),
                      'energia_util': metricas.energia_util_conservada(
                          ref, y, p['fs_hz'], bandas),
                      'atenuacion_db': atenuacion})
    return filas


def estimar_costo(p, diagnostico):
    """Cuentas teoricas con FFT compleja radix-2 y memoria float32 estimada."""
    n, fs = p['n_bloque'], p['fs_hz']
    salto = int(n * (1-p['solapamiento']))
    sos = disenar_temporal(diagnostico, p)
    secciones = 0 if sos is None else len(sos)
    b = p['b']['bytes_real_micro']
    # Una mariposa usa 4 productos y 6 sumas reales, sin optimizar giros triviales.
    mariposas = int(n // 2 * np.log2(n))
    return {
        'temporal': {'multiplicaciones_bloque': 5*secciones*n,
                     'sumas_bloque': 4*secciones*n,
                     'memoria_bytes_estimada': (7*secciones+2*n)*b,
                     'espera_adquisicion_ms': 1000*n/fs},
        'espectral': {'multiplicaciones_bloque': 8*mariposas+4*n,
                      'sumas_bloque': 12*mariposas+2*n,
                      'divisiones_bloque': n,
                      'memoria_bytes_estimada': 12*n*b,
                      'espera_adquisicion_ms': 1000*n/fs,
                      'avance_ms': 1000*salto/fs},
        'nodo_1': {'multiplicaciones_bloque': 4*mariposas+n,
                   'sumas_fft_bloque': 6*mariposas,
                   'nota': 'No incluye magnitudes, busqueda ni promedio de captura'},
    }


def correr_modo(p, identificador):
    """Ejecuta ambos metodos sobre la misma captura y conserva las evidencias."""
    limpia, contaminada = entrada(p, identificador)
    fisica = cadena_fisica(contaminada, p)
    medida = fisica['senal']
    diagnostico = nodo_1_adquisicion(medida, p)
    salidas, tiempos = {}, {}
    for nombre, funcion in [('temporal', nodo_2_temporal), ('espectral', nodo_3_espectral)]:
        inicio = perf_counter()
        salidas[nombre] = funcion(medida, diagnostico, p)
        tiempos[nombre] = 1000 * (perf_counter()-inicio)
    filas = evaluar(limpia, medida, salidas, p, diagnostico)
    for fila in filas:
        fila['modo'] = identificador
    return {'modo': identificador, 'diagnostico': diagnostico, 'metricas': filas,
            'costo': estimar_costo(p, diagnostico), 'tiempo_python_captura_ms': tiempos,
            'cadena': {k: fisica[k] for k in ('recortes_pwm', 'recortes_adc', 'rc_activo')},
            'senales': {'limpia': limpia, 'contaminada': contaminada,
                        'adquirida': medida, **salidas}}


def guardar_resultados(resultados, p):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt

    carpeta, figuras = P.CARPETA_RESULTADOS, P.CARPETA_FIGURAS
    figuras.mkdir(parents=True, exist_ok=True)
    filas = [f for r in resultados for f in r['metricas']]
    with (carpeta / 'b_metricas.csv').open('w', newline='') as archivo:
        escritor = csv.DictWriter(archivo, fieldnames=list(filas[0]))
        escritor.writeheader()
        escritor.writerows(filas)
    resumen = {'parametros': p, 'entrada': 'sustituto local de A, fases cero',
               'resultados': [{k: v for k, v in r.items() if k != 'senales'}
                              for r in resultados]}
    (carpeta / 'b_resultados.json').write_text(
        json.dumps(resumen, indent=2, allow_nan=False) + '\n')
    for r in resultados:
        np.savez_compressed(carpeta / f'b_modo_{r["modo"]}.npz', **r['senales'])
        fig, ejes = plt.subplots(2, 1, figsize=(10, 7), constrained_layout=True)
        inicio = p['b']['recorte_evaluacion_bloques'] * p['n_bloque']
        fin = inicio + int(p['b']['vista_temporal_ms'] * p['fs_hz'] / 1000)
        for nombre, x in r['senales'].items():
            ejes[0].plot(np.arange(inicio, fin)/p['fs_hz']*1000, x[inicio:fin],
                         label=nombre, linewidth=1, alpha=0.8)
            bloque = x[inicio:inicio+p['n_bloque']]
            w = np.hanning(len(bloque))
            amplitud = 2*np.abs(np.fft.rfft(bloque*w))/w.sum()
            ejes[1].plot(np.fft.rfftfreq(len(bloque), 1/p['fs_hz']),
                         20*np.log10(np.maximum(amplitud, 1e-12)), label=nombre)
        ejes[0].set(xlabel='Tiempo (ms)', ylabel='Amplitud normalizada',
                    title=f'Modo {r["modo"]}: simulacion provisional del Bloque B')
        ejes[1].set(xlabel='Frecuencia (Hz)', ylabel='Amplitud (dB ref. 1)', ylim=(-110, 5))
        for eje in ejes:
            eje.grid(alpha=0.25)
            eje.legend(ncol=3, fontsize=8)
        fig.savefig(figuras / f'b_modo_{r["modo"]}.png', dpi=160)
        plt.close(fig)
    fig, eje = plt.subplots(figsize=(11, 2.8), constrained_layout=True)
    eje.axis('off')
    cajas = [(0.07, 0.5, 'Emisor\nsustituto A'), (0.27, 0.5, 'PWM / RC / ADC\nmodelo promedio'),
             (0.49, 0.5, 'Nodo 1\nDC + FFT + diagnostico'),
             (0.71, 0.8, 'Nodo 2\nIIR notch'), (0.71, 0.2, 'Nodo 3\nFFT / IFFT'),
             (0.94, 0.5, 'Metricas')]
    for x, y, texto in cajas:
        eje.text(x, y, texto, ha='center', va='center', fontsize=9,
                 bbox={'boxstyle': 'round', 'facecolor': '#e8f1fa'})
    for origen, destino in [(0, 1), (1, 2), (2, 3), (2, 4), (3, 5), (4, 5)]:
        x1, y1, _ = cajas[origen]
        x2, y2, _ = cajas[destino]
        eje.annotate('', xy=(x2-0.07, y2), xytext=(x1+0.07, y1),
                     arrowprops={'arrowstyle': '->'})
    fig.savefig(figuras / 'b_arquitectura.png', dpi=160)
    plt.close(fig)
    lineas = [r'\begin{tabular}{llrrrrr}', r'\toprule',
              r'Modo & Metodo & MSE & SNR sal. & $\Delta$SNR & $E_u$ & Aten. \\',
              r'\midrule']
    for fila in filas:
        at = '--' if fila['atenuacion_db'] is None else f"{fila['atenuacion_db']:.2f}"
        lineas.append(f"{fila['modo']} & {fila['metodo']} & {fila['mse']:.2e} & "
                      f"{fila['snr_salida_db']:.2f} & {fila['delta_snr_db']:.2f} & "
                      f"{fila['energia_util']:.4f} & {at}" + r' \\')
    lineas += [r'\bottomrule', r'\end{tabular}']
    (carpeta / 'b_tabla_metricas.tex').write_text('\n'.join(lineas)+'\n')


def main():
    p = cargar()
    resultados = []
    print('[TODO A/C/D] Entrada local, RC opcional y margenes locales; ver comentarios')
    for modo in p['emisor']['modos']:
        resultado = correr_modo(p, modo['id'])
        resultados.append(resultado)
        print(f"[ok] modo {modo['id']}: {resultado['diagnostico']['tipo']}")
    guardar_resultados(resultados, p)
    print('[ok] Figuras, capturas, metricas y costos en taller_semana12/resultados')


if __name__ == '__main__':
    main()

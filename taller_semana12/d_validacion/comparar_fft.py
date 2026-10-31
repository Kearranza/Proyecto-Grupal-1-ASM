"""BLOQUE D - Comparacion de la FFT del micro contra la referencia de Python.

Para el taller basta con dejar definida la comparacion (la usan los diagramas
02 y 04). Para el proyecto, este script recibe el espectro que manda la Pico W
y decide si el bloque pasa o no con los margenes de parametros.json.

    python taller_semana12/d_validacion/comparar_fft.py espectro_pico.csv --modo 2
    python taller_semana12/d_validacion/comparar_fft.py --modo 2 --simular

El archivo del micro es un CSV con una fila por pico, en el orden en que viajan
en la trama del diagrama 03:

    bloque,modo,bin,nivel_db

Los niveles se comparan en relativo, no en absoluto: entre el emisor y el ADC
hay una ganancia desconocida (nivel del PWM, caida del RC, escala del ADC), asi
que los dos espectros se normalizan a su pico mas alto antes de comparar.
"""
import argparse
import csv
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comun import parametros as P  # noqa: E402
from comun.pasos import ejecutar  # noqa: E402

P.usar_src()


def margenes(p):
    """Leer y validar los margenes. Deben estar definidos antes de medir."""
    v = p['validacion']
    faltan = [k for k in ('margen_frecuencia_bins', 'margen_amplitud_db',
                          'snr_minima_captura_db') if v.get(k) is None]
    if faltan:
        raise NotImplementedError(f'TODO (D): definir en parametros.json {faltan}')
    if any(not np.isfinite(v[k]) or v[k] < 0 for k in
           ('margen_frecuencia_bins', 'margen_amplitud_db', 'snr_minima_captura_db')):
        raise ValueError('Los margenes deben ser finitos y no negativos')
    return v


def crc16_ccitt(datos, semilla=0xFFFF):
    # Referencia del CRC del diagrama 03. La Pico W usa este mismo polinomio
    crc = semilla
    for byte in datos:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def bin_de(frecuencia, p):
    return int(round(frecuencia / P.resolucion_hz(p)))


def espectro_referencia(p, identificador):
    """|X[k]| en dB de senal_del_modo() con la misma ventana y N que el micro."""
    from a_senales.prototipo_frecuencias import senal_del_modo
    _, x = senal_del_modo(p, identificador)
    x = x - np.mean(x)
    ventana = np.hanning(x.size)
    magnitud = np.abs(np.fft.rfft(x * ventana))
    return 20*np.log10(magnitud + 1e-20)


def picos_esperados(p, identificador, referencia):
    """Bin y nivel relativo que deberia tener cada componente del modo."""
    modo = P.modo(p, identificador)
    bins = [bin_de(f, p) for f in modo['frecuencias_hz']]
    niveles = [referencia[k] for k in bins]
    tope = max(niveles)
    return [{'frecuencia_hz': f, 'bin': k, 'nivel_rel_db': n - tope}
            for f, k, n in zip(modo['frecuencias_hz'], bins, niveles)]


def leer_espectro_micro(ruta):
    """Espectro que reporta la Pico W (formato del diagrama 03)."""
    if ruta is None:
        raise NotImplementedError('falta la captura del micro: la entrega el bloque C')
    with open(ruta, encoding='utf-8') as archivo:
        filas = list(csv.DictReader(archivo))
    if not filas:
        raise ValueError(f'{ruta} no trae ningun pico')
    return [{'bloque': int(f['bloque']), 'modo': int(f['modo']),
             'bin': int(f['bin']), 'nivel_db': float(f['nivel_db'])} for f in filas]


def simular_captura(p, identificador, referencia, corrimiento=0, atenuacion_db=0.0):
    # Captura de mentira para probar la regla de decision sin placa.
    # No es una medicion: sirve para verificar que decidir() marca lo que debe
    esperados = picos_esperados(p, identificador, referencia)
    return [{'bloque': 1, 'modo': identificador,
             'bin': e['bin'] + corrimiento,
             'nivel_db': e['nivel_rel_db'] - atenuacion_db*(i > 0)}
            for i, e in enumerate(esperados)]


def decidir(referencia, medido, v, p=None, identificador=None, verbose=True):
    """Pasa / no pasa por cada pico, con el error en bins y en dB."""
    esperados = picos_esperados(p, identificador, referencia)
    if not medido:
        return {'pasa': False, 'picos': [], 'motivo': 'bloque sin picos'}
    if len({m['bloque'] for m in medido}) != 1 or any(m['modo'] != identificador for m in medido):
        raise ValueError('La decision requiere un solo bloque del modo solicitado')
    if any(not np.isfinite(m['nivel_db']) or not 0 <= m['bin'] < len(referencia) for m in medido):
        raise ValueError('Pico no finito o fuera de la FFT')
    if len({m['bin'] for m in medido}) != len(medido):
        raise ValueError('Bins duplicados en un bloque')
    tope = max(m['nivel_db'] for m in medido)
    relativos = [{**m, 'nivel_rel_db': m['nivel_db'] - tope} for m in medido]

    filas, pasa_todo = [], True
    usados = set()
    for e in esperados:
        cerca = [m for m in relativos
                 if m['bin'] not in usados and abs(m['bin'] - e['bin']) <= v['margen_frecuencia_bins']]
        if not cerca:
            filas.append({**e, 'error_bins': None, 'error_db': None,
                          'pasa': False, 'motivo': 'no aparece el pico'})
            pasa_todo = False
            continue
        m = min(cerca, key=lambda c: abs(c['bin'] - e['bin']))
        usados.add(m['bin'])
        error_bins = m['bin'] - e['bin']
        error_db = m['nivel_rel_db'] - e['nivel_rel_db']
        ok = bool(abs(error_db) <= v['margen_amplitud_db'])
        filas.append({**e, 'error_bins': error_bins, 'error_db': error_db,
                      'pasa': ok, 'motivo': '' if ok else 'nivel fuera de margen'})
        pasa_todo = pasa_todo and ok

    if verbose:
        print(f"\n  {'frecuencia':>12s} {'bin esp.':>9s} {'error (bins)':>13s} "
              f"{'error (dB)':>11s} {'':>8s}")
        for f in filas:
            eb = '--' if f['error_bins'] is None else f"{f['error_bins']:+d}"
            ed = '--' if f['error_db'] is None else f"{f['error_db']:+.2f}"
            print(f"  {f['frecuencia_hz']:>9d} Hz {f['bin']:>9d} {eb:>13s} {ed:>11s} "
                  f"  {'PASA' if f['pasa'] else 'NO PASA'}  {f['motivo']}")
        print(f"\n  Resultado del bloque: {'PASA' if pasa_todo else 'NO PASA'}")
    return {'pasa': pasa_todo, 'picos': filas}


def decidir_captura(referencia, medido, v, p, identificador):
    """Separa el CSV por bloque para no mezclar picos de capturas diferentes."""
    grupos = {}
    for pico in medido:
        grupos.setdefault(pico['bloque'], []).append(pico)
    decisiones = []
    for numero, picos in grupos.items():
        print(f'Bloque {numero}')
        decisiones.append({'bloque': numero,
                           **decidir(referencia, picos, v, p, identificador)})
    return {'pasa': bool(decisiones) and all(d['pasa'] for d in decisiones),
            'bloques': decisiones}


def main():
    p = P.cargar()
    ap = argparse.ArgumentParser()
    ap.add_argument('espectro', nargs='?')
    ap.add_argument('--modo', type=int, default=2)
    ap.add_argument('--simular', action='store_true',
                    help='captura de prueba sin placa, para verificar la regla')
    args = ap.parse_args()

    estado = {}

    def leer():
        if args.simular:
            print('  (captura simulada: NO es una medicion, solo prueba la regla)')
            return simular_captura(p, args.modo, estado['ref'])
        return leer_espectro_micro(args.espectro)

    ejecutar(f'Bloque D - validacion del receptor (modo {args.modo})', [
        ('margenes definidos', lambda: estado.update(v=margenes(p))),
        ('espectro de referencia', lambda: estado.update(ref=espectro_referencia(p, args.modo))),
        ('espectro del micro', lambda: estado.update(med=leer())),
        ('decision', lambda: decidir_captura(estado['ref'], estado['med'], estado['v'],
                                             p, args.modo)),
    ], encadenado=True)


if __name__ == '__main__':
    main()

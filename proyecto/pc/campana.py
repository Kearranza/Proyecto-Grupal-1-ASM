"""Campana de pruebas P1-P5 con repeticiones (Kevin).

Lee config/pruebas.json y, por cada prueba y repeticion, toma lo que registro
registrar.py y calcula las metricas con src/metricas.py (las mismas de la Tarea 1).
Los metodos de filtrado tambien son los de la Tarea 1 corridos aqui sobre lo
capturado: esa es la validacion independiente, no se reescribe nada.

    python proyecto/pc/campana.py              # mide con la evidencia que exista
    python proyecto/pc/campana.py --autoprueba # revisa la maquinaria sin placas

Escribe resultados/campana.csv, resultados/tabla_campana.tex (la que entra al
paper) y las figuras resultados/figuras/campana_*.png.
"""
import csv
import json
import shutil
import sys
import tempfile
import wave
from pathlib import Path

import numpy as np
from matplotlib.figure import Figure

PROYECTO = Path(__file__).resolve().parent.parent
RAIZ = PROYECTO.parent
sys.path.insert(0, str(RAIZ / 'src'))
sys.path.insert(0, str(PROYECTO / 'pc'))
from pasos import ejecutar  # noqa: E402

PRUEBAS = json.loads((PROYECTO / 'config' / 'pruebas.json').read_text(encoding='utf-8'))
SISTEMA = json.loads((PROYECTO / 'config' / 'sistema.json').read_text(encoding='utf-8'))
FS = float(SISTEMA['muestreo']['fs_hz'])
EVIDENCIA = PROYECTO / PRUEBAS['evidencia']['carpeta']
SALIDAS = PROYECTO / 'resultados'
FIGURAS = SALIDAS / 'figuras'
METODOS = ('temporal', 'espectral')
COLOR = {'entrada': '#eb6834', 'temporal': '#2a78d6', 'espectral': '#1baf7a'}

# Todo sale de los JSON: aqui no se escribe ninguna frecuencia a mano
EMISOR = PRUEBAS['emisor']
ANCHO_TONO_HZ = EMISOR['ancho_tono_hz']
BANDA_UTIL_AUDIO = tuple(PRUEBAS['bandas']['banda_util_audio_hz'])
BANDA_FUERA = tuple(PRUEBAS['bandas']['banda_fuera_hz'])
# La suma solapada no normaliza el primer ni el ultimo medio bloque, asi que ahi la
# salida tiene un transitorio que no es comportamiento del filtro. Se recorta un bloque
# de cada punta, igual en referencia, entrada y salida, para que la comparacion siga
# siendo justa. Sobre 32768 muestras es el 6 %.
MARGEN = int(SISTEMA['bloque']['n_por_defecto'])
# El retardo mas largo que se le compensa a un filtro: 8 ms, de sobra para el IIR
RETARDO_MAX = MARGEN // 8
# El nodo 1 manda cuentas del ADC centradas: 2^(bits-1) es el fondo de escala, no 32768.
# Las metricas son relativas y no cambian, pero el numero crudo si.
FONDO_ESCALA = float(1 << (SISTEMA['adc']['bits'] - 1))

filas = []


def verificar_criterios():
    faltan = [k for k, v in PRUEBAS['criterios_aceptacion'].items()
              if not k.startswith('_') and v is None]
    if faltan:
        raise NotImplementedError(f'faltan por fijar: {faltan}')


def criterios_de(prueba):
    # Los criterios de la prueba pisan a los generales
    base = {k: v for k, v in PRUEBAS['criterios_aceptacion'].items() if not k.startswith('_')}
    base.update({k: v for k, v in prueba.get('criterios', {}).items() if not k.startswith('_')})
    return base


def escribir_tabla_criterios():
    """Tabla de criterios para el paper, generada del JSON para que no se desincronice."""
    SALIDAS.mkdir(parents=True, exist_ok=True)
    lineas = ['% generada por proyecto/pc/campana.py a partir de config/pruebas.json',
              '\\begin{tabular}{llrrr}', '\\toprule',
              'Prueba & Condición & $\\Delta$SNR mín. & $E_{\\text{útil}}$ mín. & Aten. mín. \\\\',
              ' & & (dB) & (\\%) & (dB) \\\\', '\\midrule']
    for prueba in PRUEBAS['pruebas']:
        c = criterios_de(prueba)
        aten = (f"{c['atenuacion_interferencia_min_db']:.0f}"
                if 'atenuacion_interferencia_min_db' in prueba.get('criterios', {}) else '---')
        lineas.append(f"{prueba['id']} & {prueba['condicion']} & "
                      f"{c['delta_snr_min_db']:.0f} & {c['energia_util_min_pct']:.0f} & {aten} \\\\")
    lineas += ['\\bottomrule', '\\end{tabular}']
    (SALIDAS / 'tabla_criterios.tex').write_text('\n'.join(lineas) + '\n', encoding='utf-8')


def tonos_utiles_de(prueba):
    """Tonos que cuentan como senal util; en el modo 4 el ultimo es la interferencia."""
    modo = prueba.get('modo_emisor')
    if not modo:
        return None
    cuantos = EMISOR['tonos_utiles_del_modo'][modo]
    datos = EMISOR['modos'][modo]
    return datos['frecuencias_hz'][:cuantos], datos['amplitudes'][:cuantos]


def bandas_utiles_de(prueba):
    import metricas
    tonos = tonos_utiles_de(prueba)
    if tonos is None:
        return [BANDA_UTIL_AUDIO]
    return metricas.bandas_utiles_de(tonos[0], ANCHO_TONO_HZ)


def leer_wav(ruta):
    with wave.open(str(ruta), 'rb') as archivo:
        fs = archivo.getframerate()
        crudo = archivo.readframes(archivo.getnframes())
    return np.frombuffer(crudo, dtype='<i2').astype(float)/32768.0, fs


def referencia_capturada(prueba, carpeta):
    """La pasada en la que la emisora toco LIMPIA, grabada por el mismo camino.

    Con el microfono de por medio, el parlante y el cuarto colorean lo que llega. El WAV
    ya no sirve de referencia: lo que sirve es la misma senal limpia pasada por la misma
    cadena. Lo que el cuarto agrega esta en las dos capturas y se cancela al comparar.
    """
    patron = PRUEBAS['evidencia'].get('patron_referencia')
    if not patron:
        return None
    base = carpeta / patron.format(id=prueba['id'])
    if not base.with_suffix('.npz').exists():
        return None
    return np.load(base.with_suffix('.npz'))['muestras'].astype(float)/FONDO_ESCALA


def referencia_de(prueba, n_muestras):
    """Referencia de la ruta electrica: sintetizada para P1 y P2, del WAV para P3-P5."""
    tonos = tonos_utiles_de(prueba)
    if tonos is not None:
        import senales
        frecuencias, amplitudes = tonos
        t = np.arange(n_muestras)/FS
        x = senales.multitono(t, frecuencias, amplitudes, [0.0]*len(frecuencias))
        return x/np.max(np.abs(x))
    if not prueba.get('wav'):
        raise NotImplementedError(
            f"{prueba['id']} no tiene ni modo_emisor ni wav en pruebas.json")
    ruta = PROYECTO / prueba['wav']
    if not ruta.exists():
        raise NotImplementedError(f'falta {ruta.relative_to(RAIZ)}')
    x, fs = leer_wav(ruta)
    if abs(fs - FS) > 1:
        raise ValueError(f'{ruta.name} esta a {fs} Hz y el sistema trabaja a {FS:.0f} Hz')
    if x.size < n_muestras:
        raise ValueError(f'{ruta.name} tiene {x.size} muestras y se capturaron {n_muestras}')
    return x[:n_muestras]


def solo_banda(x, bandas):
    # Deja pasar unicamente las bandas utiles, por mascara en frecuencia
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(x.size, 1/FS)
    mascara = np.zeros(f.size, dtype=bool)
    for bajo, alto in bandas:
        mascara |= (f >= bajo) & (f <= alto)
    return np.fft.irfft(X*mascara, x.size)


def alinear(referencia, medido, bandas):
    """Compensa desfase y ganancia: la cadena PWM-RC-ADC tiene retardo y escala desconocidos.

    La ganancia se estima solo dentro de la banda util; si se estimara sobre todo el
    ancho de banda, una interferencia fuerte se la llevaria y la referencia quedaria
    descalibrada.
    """
    n = min(referencia.size, medido.size)
    referencia = referencia[:n] - np.mean(referencia[:n])
    medido = medido[:n] - np.mean(medido[:n])
    tamano = 1
    while tamano < 2*n:
        tamano <<= 1
    correlacion = np.fft.irfft(np.fft.rfft(medido, tamano)*np.conj(np.fft.rfft(referencia, tamano)),
                               tamano)
    desfase = int(np.argmax(np.abs(correlacion)))
    if desfase > tamano//2:
        desfase -= tamano
    if desfase >= 0:
        referencia, medido = referencia[:n-desfase], medido[desfase:n]
    else:
        referencia, medido = referencia[-desfase:n], medido[:n+desfase]
    r_banda, m_banda = solo_banda(referencia, bandas), solo_banda(medido, bandas)
    escala = float(np.dot(r_banda, m_banda)/(np.dot(m_banda, m_banda) + 1e-20))
    return referencia, medido*escala, desfase


def frecuencias_utiles_de(prueba):
    """Rejilla que cubre la banda util, del paso de un bin.

    El filtro espectral protege una lista de frecuencias y ataca el pico mas fuerte
    fuera de ellas. Con tonos basta con los tonos, pero una senal de banda como la de
    P3 necesita toda su banda declarada: si no, el filtro elige la propia senal como
    interferencia. Es lo mismo que en la placa se declara con UTIL y ANCHO_UTIL.
    """
    tonos = tonos_utiles_de(prueba)
    if tonos is not None:
        return tonos[0]
    paso = 2*FS/SISTEMA['bloque']['n_por_defecto']       # dos bins: el margen que usa
    bajo, alto = BANDA_UTIL_AUDIO
    return list(np.arange(bajo, alto + paso, paso))


def filtrar(entrada, metodo, prueba):
    """Corre el metodo de la Tarea 1 sobre lo capturado."""
    if metodo == 'espectral':
        import filtrado_espectral
        previas = filtrado_espectral.FRECUENCIAS_UTILES
        filtrado_espectral.FRECUENCIAS_UTILES = frecuencias_utiles_de(prueba)
        try:
            return np.asarray(filtrado_espectral.reducir_ruido(entrada, FS))[:entrada.size]
        finally:
            filtrado_espectral.FRECUENCIAS_UTILES = previas
    try:
        import filtros_temporales as ft
    except ImportError as e:
        raise NotImplementedError(f'el metodo temporal necesita scipy en esta maquina ({e})')
    sos, _, _ = ft.disenar_iir(FS, wp=list(BANDA_UTIL_AUDIO), ws=[200.0, 2200.0],
                         gpass=1.0, gstop=40.0)
    return np.asarray(ft.filtrar_sos_manual(sos, entrada))[:entrada.size]


def recortar(*senales):
    # Quita el transitorio de la suma solapada de las dos puntas
    n = min(s.size for s in senales)
    if n <= 2*MARGEN:
        raise ValueError(f'hacen falta mas de {2*MARGEN} muestras para medir, hay {n}')
    return [s[MARGEN:n-MARGEN] for s in senales]


def compensar_retardo(limpia, entrada, salida):
    # El IIR atrasa la salida (~27 muestras en P3) y sin esto el MSE mide el retardo,
    # no el filtrado. La latencia se reporta aparte, con la de las placas.
    n = min(limpia.size, entrada.size, salida.size)
    tamano = 1
    while tamano < 2*n:
        tamano <<= 1
    correlacion = np.fft.irfft(np.fft.rfft(salida[:n], tamano)*np.conj(np.fft.rfft(limpia[:n], tamano)),
                               tamano)
    retardo = int(np.argmax(correlacion[:RETARDO_MAX + 1]))   # causal: solo atrasos
    return limpia[:n-retardo], entrada[:n-retardo], salida[retardo:n], retardo


def medir(limpia, entrada, salida, bandas):
    import metricas
    limpia, entrada, salida, retardo = compensar_retardo(limpia, entrada, salida)
    limpia, entrada, salida = recortar(limpia, entrada, salida)
    return {
        'retardo_filtro_muestras': retardo,
        'mse': metricas.mse(limpia, salida),
        'snr_db': metricas.snr_db(limpia, salida),
        'delta_snr_db': metricas.delta_snr_db(limpia, entrada, salida),
        'energia_util_pct': 100.0*metricas.energia_util_conservada(limpia, salida, FS, bandas),
        'atenuacion_db': metricas.atenuacion_db(entrada, salida, FS, BANDA_FUERA),
    }


def latencia_de(contadores):
    # El nodo 1 reporta la latencia en las tramas de ESTADO; se toma la ultima
    estados = contadores.get('estado') or []
    return estados[-1].get('latencia_ms', '') if estados else ''


def medir_prueba(prueba, carpeta=None):
    carpeta = carpeta or EVIDENCIA
    patron = PRUEBAS['evidencia']['patron']
    hechas, pendientes = 0, []
    for repeticion in range(1, int(PRUEBAS['repeticiones']) + 1):
        base = carpeta / patron.format(id=prueba['id'], repeticion=repeticion)
        if not base.with_suffix('.npz').exists():
            continue
        datos = np.load(base.with_suffix('.npz'))
        contadores = json.loads(base.with_suffix('.json').read_text(encoding='utf-8'))
        capturada = datos['muestras'].astype(float)/FONDO_ESCALA
        if capturada.size < 4096:
            raise ValueError(f'{base.name}: solo {capturada.size} muestras')
        bandas = bandas_utiles_de(prueba)
        ref = referencia_capturada(prueba, carpeta)
        if ref is None:
            ref = referencia_de(prueba, capturada.size)
            origen = 'electrica'
        else:
            origen = 'acustica'
        limpia, entrada, desfase = alinear(ref, capturada, bandas)
        for metodo in METODOS:
            try:
                salida = filtrar(entrada, metodo, prueba)
            except NotImplementedError as e:
                pendientes.append(str(e))
                continue
            filas.append({'prueba': prueba['id'], 'repeticion': repeticion, 'metodo': metodo,
                          'ruta': origen, 'desfase_muestras': desfase,
                          'errores_crc': contadores.get('errores_crc', 0),
                          'bloques_perdidos': contadores.get('bloques_perdidos', 0),
                          'latencia_ms': latencia_de(contadores),
                          **medir(limpia, entrada, salida, bandas)})
            if repeticion == 1:
                e_fig, s_fig = recortar(entrada, salida)
                figura(prueba, e_fig, {metodo: s_fig})
        hechas += 1
    if not hechas:
        ejemplo = (carpeta / patron.format(id=prueba['id'], repeticion=1)).relative_to(RAIZ)
        raise NotImplementedError(f'falta la evidencia: registrar.py --salida {ejemplo}')
    if PRUEBAS.get('ruta') == 'acustica' and referencia_capturada(prueba, carpeta) is None:
        ref = (carpeta / PRUEBAS['evidencia']['patron_referencia'].format(
            id=prueba['id'])).relative_to(RAIZ)
        raise NotImplementedError(
            f'medido con el WAV, no con la cadena real: falta la pasada LIMPIA en {ref}')
    if pendientes:
        raise NotImplementedError(f'{hechas} repeticion(es) medidas, pero {pendientes[0]}')


def figura(prueba, entrada, salidas):
    """Espectro de lo capturado contra lo filtrado, para el paper."""
    import metricas
    FIGURAS.mkdir(parents=True, exist_ok=True)
    fig = Figure(figsize=(7.0, 3.2), dpi=150)
    eje = fig.add_subplot(111)
    f, magnitud_db, _ = metricas.espectro(entrada, FS)
    eje.plot(f, magnitud_db, color=COLOR['entrada'], lw=0.7, label='capturada')
    for metodo, salida in salidas.items():
        f, magnitud_db, _ = metricas.espectro(salida, FS)
        eje.plot(f, magnitud_db, color=COLOR[metodo], lw=0.7, label=metodo)
    for bajo, alto in bandas_utiles_de(prueba):
        eje.axvspan(bajo, alto, color='#2a78d6', alpha=0.08)
    eje.set_xlim(0, min(5000.0, FS/2))
    eje.set_xlabel('Frecuencia (Hz)')
    eje.set_ylabel('Magnitud (dB)')
    eje.set_title(f"{prueba['id']}: {prueba['senal']}")
    eje.legend(loc='upper right', fontsize=8)
    eje.grid(alpha=0.2)
    fig.tight_layout()
    fig.savefig(FIGURAS / f"campana_{prueba['id'].lower()}.png")


def agregar():
    """Promedio y desviacion por prueba y metodo, con el veredicto de los criterios."""
    resumen_filas = []
    for prueba in PRUEBAS['pruebas']:
        criterios = criterios_de(prueba)
        for metodo in METODOS:
            propias = [f for f in filas if f['prueba'] == prueba['id'] and f['metodo'] == metodo]
            if not propias:
                continue
            fila = {'prueba': prueba['id'], 'metodo': metodo, 'repeticiones': len(propias)}
            for clave in ('delta_snr_db', 'snr_db', 'mse', 'energia_util_pct', 'atenuacion_db'):
                valores = np.array([f[clave] for f in propias], dtype=float)
                fila[clave] = round(float(valores.mean()), 6)
                fila[clave + '_desv'] = round(float(valores.std(ddof=1)) if valores.size > 1
                                              else 0.0, 6)
            fila['retardo_filtro_muestras'] = max(f['retardo_filtro_muestras'] for f in propias)
            fila['errores_crc'] = sum(f['errores_crc'] for f in propias)
            fila['bloques_perdidos'] = sum(f['bloques_perdidos'] for f in propias)
            fila['cumple'] = bool(
                fila['delta_snr_db'] >= criterios['delta_snr_min_db'] and
                fila['energia_util_pct'] >= criterios['energia_util_min_pct'])
            resumen_filas.append(fila)
    return resumen_filas


def escribir_tabla(agregado):
    lineas = ['% generada por proyecto/pc/campana.py',
              '\\begin{tabular}{llrrrrc}', '\\toprule',
              'Prueba & Método & $\\Delta$SNR (dB) & MSE & $E_{\\text{útil}}$ (\\%) '
              '& Aten. (dB) & Cumple \\\\', '\\midrule']
    for f in agregado:
        lineas.append(
            f"{f['prueba']} & {f['metodo']} & "
            f"${f['delta_snr_db']:.2f} \\pm {f['delta_snr_db_desv']:.2f}$ & "
            f"${f['mse']:.2e}$ & {f['energia_util_pct']:.1f} & {f['atenuacion_db']:.1f} & "
            f"{'sí' if f['cumple'] else 'no'} \\\\")
    lineas += ['\\bottomrule', '\\end{tabular}']
    (SALIDAS / 'tabla_campana.tex').write_text('\n'.join(lineas) + '\n', encoding='utf-8')


def resumen():
    if not filas:
        raise NotImplementedError('no hay ninguna medicion todavia')
    agregado = agregar()
    SALIDAS.mkdir(parents=True, exist_ok=True)
    with open(SALIDAS / 'campana.csv', 'w', newline='', encoding='utf-8') as archivo:
        escritor = csv.DictWriter(archivo, fieldnames=list(agregado[0]), lineterminator='\n')
        escritor.writeheader()
        escritor.writerows(agregado)
    escribir_tabla(agregado)
    print(f"\n  {'prueba':>6s} {'metodo':>10s} {'dSNR (dB)':>17s} {'E util (%)':>11s}  cumple")
    for f in agregado:
        print(f"  {f['prueba']:>6s} {f['metodo']:>10s} "
              f"{f['delta_snr_db']:>9.2f} +- {f['delta_snr_db_desv']:<4.2f} "
              f"{f['energia_util_pct']:>11.1f}  {'si' if f['cumple'] else 'NO'}")
    print(f'\n  escritos {SALIDAS.name}/campana.csv, {SALIDAS.name}/tabla_campana.tex '
          f'y {len(list(FIGURAS.glob("campana_*.png")))} figura(s)')


def _guardar_falsa(base, x, n):
    np.savez(base.with_suffix('.npz'),
             muestras=np.clip(x*32767, -32768, 32767).astype('<i2'),
             seq=np.arange(n//512, dtype='<u2'))
    base.with_suffix('.json').write_text(json.dumps(
        {'errores_crc': 0, 'bloques_perdidos': 0,
         'estado': [{'latencia_ms': 42.0}]}), encoding='utf-8')


def evidencia_sintetica(carpeta, prueba, repeticiones):
    """Evidencia inventada para revisar la maquinaria: no sirve como resultado.

    Imita la ruta acustica: el parlante y el cuarto colorean igual las dos pasadas,
    asi que la de LIMPIA sirve de referencia y lo que agregan se cancela.
    """
    generador = np.random.default_rng(7)
    patron = PRUEBAS['evidencia']['patron']
    n = 32768
    limpia = referencia_de(prueba, n)
    cuarto = np.array([1.0, 0.0, 0.35, 0.0, 0.12])        # eco corto del cuarto
    colorear = lambda s: np.convolve(s, cuarto)[:n]

    base = carpeta / PRUEBAS['evidencia']['patron_referencia'].format(id=prueba['id'])
    _guardar_falsa(base, np.roll(0.3*colorear(limpia), 17), n)

    for repeticion in range(1, repeticiones + 1):
        ruido = 2.0*np.sin(2*np.pi*6000.0*np.arange(n)/FS) + 0.05*generador.standard_normal(n)
        x = np.roll(0.3*colorear(limpia + ruido), 17)
        _guardar_falsa(carpeta / patron.format(id=prueba['id'], repeticion=repeticion), x, n)


def autoprueba():
    """Revisa la cadena completa con evidencia inventada y la borra al terminar."""
    prueba = PRUEBAS['pruebas'][0]
    carpeta = Path(tempfile.mkdtemp(prefix='campana_'))
    try:
        evidencia_sintetica(carpeta, prueba, int(PRUEBAS['repeticiones']))
        medir_prueba(prueba, carpeta)
    except NotImplementedError as e:
        print(f'  (aviso) {e}')
    finally:
        shutil.rmtree(carpeta, ignore_errors=True)
    propias = [f for f in filas if f['prueba'] == prueba['id']]
    if len(propias) < int(PRUEBAS['repeticiones']):
        raise AssertionError(f'se esperaban al menos {PRUEBAS["repeticiones"]} filas')
    if propias[0]['ruta'] != 'acustica':
        raise AssertionError('no uso la referencia capturada')
    if abs(propias[0]['desfase_muestras']) > 2:
        raise AssertionError(f"desfase mal estimado: {propias[0]['desfase_muestras']}")
    if propias[0]['delta_snr_db'] <= 0:
        raise AssertionError('el filtrado deberia mejorar la SNR de la senal inventada')
    print(f"  (autoprueba) ruta {propias[0]['ruta']}, desfase {propias[0]['desfase_muestras']}, "
          f"dSNR {propias[0]['delta_snr_db']:.1f} dB en {len(propias)} fila(s)")


def main():
    if '--autoprueba' in sys.argv:
        global SALIDAS, FIGURAS
        temporal = Path(tempfile.mkdtemp(prefix='campana_salidas_'))
        SALIDAS, FIGURAS = temporal, temporal/'figuras'   # no ensuciar resultados/
        try:
            ejecutar('Campana: autoprueba de la maquinaria',
                     [('criterios fijados antes de medir', verificar_criterios),
                      ('medicion, alineacion y metricas', autoprueba),
                      ('resumen, tabla y figuras', resumen)])
        finally:
            shutil.rmtree(temporal, ignore_errors=True)
        return
    pasos = [('criterios fijados antes de medir', verificar_criterios),
             ('tabla de criterios para el paper', escribir_tabla_criterios)]
    pasos += [(f'{p["id"]}: {p["senal"]}', lambda p=p: medir_prueba(p))
              for p in PRUEBAS['pruebas']]
    pasos.append(('resumen, tabla y figuras', resumen))
    ejecutar('Campana de pruebas', pasos)


if __name__ == '__main__':
    main()

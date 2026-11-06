# Generacion reproducible de los casos de prueba (Tarea 1 y pruebas P3-P5 del proyecto)
#
# Ampliado para el proyecto (Santiago): ruido impulsivo, zumbido de red (60 Hz con
# armonicos) y dos senales utiles nuevas (melodia y rafagas). Lo que ya existia no
# cambia: un caso que no pida nada de esto sale bit a bit igual que antes.

import json
import wave
from pathlib import Path

import numpy as np

RAIZ = Path(__file__).resolve().parent.parent
CARPETA_PRUEBAS = RAIZ / 'pruebas'


def cargar_configuracion(ruta=None):
    ruta = Path(ruta) if ruta else RAIZ / 'metadatos' / 'configuraciones.json'
    datos = json.loads(ruta.read_text(encoding='utf-8'))
    return datos['comun'], {caso['id']: caso for caso in datos['casos']}


def multitono(t, frecuencias, amplitudes, fases_grados):
    # Suma de sinusoides, cada una con su amplitud y su fase
    return sum(a * np.sin(2*np.pi*f*t + np.deg2rad(p))
               for f, a, p in zip(frecuencias, amplitudes, fases_grados))


def melodia(t, notas, armonicos=(1, 2, 3), amplitudes=(1.0, 0.4, 0.2), ataque_ms=10.0):
    # Senal tipo audio: cada nota es (frecuencia_hz, inicio_s, duracion_s) y suena como
    # un timbre simple (fundamental y armonicos) 
    x = np.zeros_like(t)
    for frecuencia, inicio, duracion in notas:
        local = t - inicio
        dentro = (local >= 0) & (local < duracion)
        tl = local[dentro]
        envolvente = (np.minimum(tl/(ataque_ms/1000), 1.0)
                      * np.minimum((duracion - tl)/0.010, 1.0)
                      * np.exp(-2.5*tl/duracion))
        timbre = sum(a * np.sin(2*np.pi*h*frecuencia*tl)
                     for h, a in zip(armonicos, amplitudes))
        x[dentro] += envolvente * timbre
    return x


def rafagas(t, eventos):
    # Senal transitoria: cada evento es (frecuencia_hz, inicio_s, duracion_ms), un tono
    # de ataque casi instantaneo (1 ms) que decae exponencialmente (constante = duracion/3)
    x = np.zeros_like(t)
    for frecuencia, inicio, duracion_ms in eventos:
        tau = duracion_ms/1000/3
        local = t - inicio
        dentro = (local >= 0) & (local < 10*tau)
        tl = local[dentro]
        x[dentro] += (np.minimum(tl/0.001, 1.0) * np.exp(-tl/tau)
                      * np.sin(2*np.pi*frecuencia*tl))
    return x


def generar_util(caso, t, fs):
    # Senal util del caso. Sin 'senal_util' en el caso es el multitono de siempre.
    tipo = caso.get('senal_util', 'multitono')
    if tipo == 'multitono':
        return multitono(t, caso['frecuencias_utiles_hz'],
                         caso['amplitudes_utiles'],
                         caso['fases_utiles_grados'])
    if tipo == 'melodia':
        return melodia(t, caso['notas'],
                       caso.get('armonicos_melodia', (1, 2, 3)),
                       caso.get('amplitudes_armonicos', (1.0, 0.4, 0.2)))
    if tipo == 'rafagas':
        return rafagas(t, caso['eventos'])
    raise ValueError(f'Senal util no reconocida: {tipo}')


def desvanecer(x, fs, milisegundos):
    # Rampas de entrada y salida para que la reproduccion no empiece con un salto
    n = min(int(fs*milisegundos/1000), x.size//2)
    if n <= 0:
        return x
    rampa = np.sin(np.linspace(0, np.pi/2, n))**2
    y = x.copy()
    y[:n] *= rampa
    y[-n:] *= rampa[::-1]
    return y


def _mascara_de_banda(f, bajo, alto, transicion):
    # Paso banda ideal con transicion suave (coseno) para que no haya ringing en el tiempo
    mascara = np.zeros_like(f)
    mascara[(f >= bajo) & (f <= alto)] = 1.0
    subida = (f > bajo - transicion) & (f < bajo)
    bajada = (f > alto) & (f < alto + transicion)
    mascara[subida] = 0.5*(1 - np.cos(np.pi*(f[subida] - bajo + transicion)/transicion))
    mascara[bajada] = 0.5*(1 + np.cos(np.pi*(f[bajada] - alto)/transicion))
    return mascara


def ruido_tonal(t, frecuencia, generador):
    return np.sin(2*np.pi*frecuencia*t + generador.uniform(-np.pi, np.pi))


def ruido_blanco(n, generador):
    return generador.standard_normal(n)


def ruido_de_banda(n, fs, banda, generador, transicion=50.0):
    # Ruido blanco al que se le conserva una sola banda enmascarando su espectro
    bajo, alto = banda
    if not 0 < bajo < alto < fs/2:
        raise ValueError(f'La banda {banda} no cabe por debajo de {fs/2} Hz')
    espectro = np.fft.rfft(generador.standard_normal(n))
    frecuencias = np.fft.rfftfreq(n, 1/fs)
    return np.fft.irfft(espectro * _mascara_de_banda(frecuencias, bajo, alto, transicion), n)


def ruido_impulsivo(n, fs, generador, tasa_hz=8.0, duracion_ms=0.5):
    # Clics al azar: llegan en instantes aleatorios (en promedio tasa_hz por segundo),
    # con signo y amplitud al azar (exponencial: de vez en cuando sale uno muy grande).
    # Cada clic es un pulso corto de media cero, (1 - k/tau) exp(-k/tau), con
    # tau = duracion_ms. La energia queda concentrada en pocas muestras, a diferencia
    # del ruido blanco, y eso es lo que lo distingue en el tiempo (curtosis alta).
    tau = max(duracion_ms*fs/1000, 1.0)
    llegada = generador.random(n) < tasa_hz/fs
    if not llegada.any():                    
        llegada[n//2] = True
    amplitud = generador.choice([-1.0, 1.0], n) * generador.exponential(1.0, n)
    impulsos = np.where(llegada, amplitud, 0.0)
    k = np.arange(int(np.ceil(8*tau)))
    pulso = (1 - k/tau) * np.exp(-k/tau)
    return np.convolve(impulsos, pulso)[:n]


def interferencia_red(t, generador, frecuencia=60.0, armonicos=(1, 2, 3, 4, 5),
                      amplitudes=(1.0, 0.5, 0.35, 0.2, 0.15)):
    # Zumbido de la red electrica: la fundamental (60 Hz) y sus armonicos
    fases = generador.uniform(-np.pi, np.pi, len(armonicos))
    return sum(a * np.sin(2*np.pi*k*frecuencia*t + p)
               for k, a, p in zip(armonicos, amplitudes, fases))


def generar_ruido(caso, t, fs, generador):
    tipo = caso['tipo_ruido']
    if tipo == 'tonal':
        return ruido_tonal(t, caso['frecuencia_interferencia_hz'], generador)
    if tipo == 'blanco':
        return ruido_blanco(t.size, generador)
    if tipo == 'banda':
        return ruido_de_banda(t.size, fs, caso['banda_ruido_hz'], generador)
    if tipo == 'impulsivo':
        return ruido_impulsivo(t.size, fs, generador,
                               caso.get('tasa_impulsos_hz', 8.0),
                               caso.get('duracion_impulso_ms', 0.5))
    if tipo == 'red':
        return interferencia_red(t, generador,
                                 caso.get('frecuencia_red_hz', 60.0),
                                 caso.get('armonicos_red', (1, 2, 3, 4, 5)),
                                 caso.get('amplitudes_red', (1.0, 0.5, 0.35, 0.2, 0.15)))
    raise ValueError(f'Tipo de ruido no reconocido: {tipo}')


def escalar_para_snr(limpia, ruido, snr_objetivo_db):
    # Ajusta la amplitud del ruido para que la SNR salga exactamente la pedida
    factor = np.sqrt(np.sum(limpia**2) /
                     (np.sum(ruido**2) * 10**(snr_objetivo_db/10)))
    return ruido * factor


def normalizar(senales, pico):
    # Una sola ganancia para todas, porque una ganancia comun no altera la SNR
    maximo = max(np.max(np.abs(x)) for x in senales)
    ganancia = pico/maximo if maximo > 0 else 1.0
    return [x*ganancia for x in senales], float(ganancia)


def verificar_nyquist(caso, fs):
    # La componente mas alta de la senal util y del ruido tiene que quedar bajo fs/2
    presentes = list(caso.get('frecuencias_utiles_hz') or [])
    if caso.get('notas'):
        presentes.append(max(n[0] for n in caso['notas'])
                         * max(caso.get('armonicos_melodia', (1, 2, 3))))
    if caso.get('eventos'):
        presentes.append(max(e[0] for e in caso['eventos']))
    if caso.get('frecuencia_interferencia_hz'):
        presentes.append(caso['frecuencia_interferencia_hz'])
    if caso.get('banda_ruido_hz'):
        presentes.append(caso['banda_ruido_hz'][1])
    if caso.get('tipo_ruido') == 'red':
        presentes.append(caso.get('frecuencia_red_hz', 60.0)
                         * max(caso.get('armonicos_red', (1, 2, 3, 4, 5))))
    if max(presentes) >= fs/2:
        raise ValueError(f"El caso {caso['id']} viola Nyquist: {max(presentes)} Hz con fs/2 = {fs/2} Hz")


def generar_caso(identificador, comun=None, casos=None):
    # Devuelve las tres senales del caso mas los datos que las describen
    if comun is None or casos is None:
        comun, casos = cargar_configuracion()
    caso = casos[identificador]
    fs = comun['fs_hz']
    n = int(round(comun['duracion_s']*fs))
    verificar_nyquist(caso, fs)

    generador = np.random.default_rng(caso['semilla'])
    t = np.arange(n)/fs
    limpia = desvanecer(generar_util(caso, t, fs), fs, comun['fade_ms'])
    ruido = escalar_para_snr(limpia, generar_ruido(caso, t, fs, generador),
                             caso['snr_objetivo_db'])
    (limpia, ruido, contaminada), ganancia = normalizar(
        [limpia, ruido, limpia + ruido], comun['pico_maximo'])

    return {'id': identificador, 'fs': fs, 'n': n, 't': t,
            'limpia': limpia, 'ruido': ruido, 'contaminada': contaminada,
            'ganancia': ganancia, 'config': caso, 'comun': comun}


def escribir_wav(ruta, x, fs):
    muestras = np.int16(np.clip(x, -1.0, 1.0) * 32767)
    with wave.open(str(ruta), 'wb') as archivo:
        archivo.setnchannels(1)
        archivo.setsampwidth(2)
        archivo.setframerate(int(fs))
        archivo.writeframes(muestras.tobytes())


def leer_wav(ruta):
    with wave.open(str(ruta), 'rb') as archivo:
        fs = archivo.getframerate()
        crudo = archivo.readframes(archivo.getnframes())
    return np.frombuffer(crudo, dtype=np.int16).astype(np.float64)/32767.0, fs


def guardar_caso(datos, carpeta=None):
    # Escribe los tres WAV y el JSON con lo que hace falta para repetir el caso
    carpeta = Path(carpeta) if carpeta else CARPETA_PRUEBAS / datos['id']
    carpeta.mkdir(parents=True, exist_ok=True)
    for nombre in ('limpia', 'ruido', 'contaminada'):
        escribir_wav(carpeta / f'{nombre}.wav', datos[nombre], datos['fs'])

    descripcion = dict(datos['config'])
    descripcion.update({
        'fs_hz': datos['fs'],
        'muestras': datos['n'],
        'duracion_s': datos['n']/datos['fs'],
        'ganancia_comun': datos['ganancia'],
        # Redondeadas: el ultimo digito de un double cambia entre plataformas y
        # eso hacia que el JSON saliera distinto en cada maquina sin cambiar nada
        'snr_obtenida_db': round(snr_db(datos['limpia'], datos['contaminada']), 6),
        'snr_tras_cuantizar_db': round(snr_db(*_leer_par(carpeta)), 6),
        'archivos': {'limpia': 'limpia.wav', 'ruido': 'ruido.wav',
                     'contaminada': 'contaminada.wav'},
    })
    (carpeta / f"{datos['id']}.json").write_text(
        json.dumps(descripcion, ensure_ascii=False, indent=2), encoding='utf-8')
    return descripcion


def _leer_par(carpeta):
    limpia, _ = leer_wav(carpeta / 'limpia.wav')
    contaminada, _ = leer_wav(carpeta / 'contaminada.wav')
    return limpia, contaminada


def snr_db(referencia, estimada):
    # Repetida aqui para que el modulo no dependa de metricas al guardar
    error = np.asarray(estimada) - np.asarray(referencia)
    return float(10*np.log10(np.sum(np.asarray(referencia)**2) /
                             (np.sum(error**2) + 1e-20)))


def cargar_caso(identificador, carpeta=None):
    # Lee del disco lo que genero este modulo, que es la entrada de los demas bloques
    carpeta = Path(carpeta) if carpeta else CARPETA_PRUEBAS / identificador
    descripcion = json.loads((carpeta / f'{identificador}.json').read_text(encoding='utf-8'))
    senales = {}
    for nombre in ('limpia', 'ruido', 'contaminada'):
        senales[nombre], fs = leer_wav(carpeta / f'{nombre}.wav')
    return {'id': identificador, 'fs': fs, 'n': senales['limpia'].size,
            't': np.arange(senales['limpia'].size)/fs,
            'config': descripcion, **senales}


def generar_todos(verbose=True):
    comun, casos = cargar_configuracion()
    resumen = []
    for identificador in casos:
        datos = generar_caso(identificador, comun, casos)
        descripcion = guardar_caso(datos)
        resumen.append(descripcion)
        if verbose:
            print(f"{identificador}: {descripcion['tipo_ruido']:7s} "
                  f"SNR objetivo {descripcion['snr_objetivo_db']:5.2f} dB | "
                  f"obtenida {descripcion['snr_obtenida_db']:7.4f} dB | "
                  f"tras cuantizar {descripcion['snr_tras_cuantizar_db']:7.4f} dB")
    return resumen


if __name__ == '__main__':
    generar_todos()
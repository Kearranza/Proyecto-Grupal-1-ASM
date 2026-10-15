# Metricas para comparar una senal de referencia con una version procesada

import numpy as np

EPSILON = 1e-20


def energia(x):
    return float(np.sum(np.square(np.asarray(x, dtype=float))))


def mse(referencia, estimada):
    # Ecuacion (1) del enunciado
    error = np.asarray(estimada, dtype=float) - np.asarray(referencia, dtype=float)
    return float(np.mean(error**2))


def snr_db(referencia, estimada):
    # Ecuacion (2): energia de la referencia sobre energia del error
    referencia = np.asarray(referencia, dtype=float)
    error = np.asarray(estimada, dtype=float) - referencia
    return float(10*np.log10(energia(referencia)/(energia(error) + EPSILON)))


def delta_snr_db(referencia, entrada, salida):
    # Ecuacion (3)
    return snr_db(referencia, salida) - snr_db(referencia, entrada)


def espectro(x, fs, ventana=True):
    # Magnitud en dB y fase de la mitad util del espectro
    x = np.asarray(x, dtype=float)
    if ventana:
        x = x * np.hanning(x.size)
    X = np.fft.rfft(x)
    frecuencias = np.fft.rfftfreq(x.size, 1/fs)
    magnitud_db = 20*np.log10(np.abs(X) + EPSILON)
    return frecuencias, magnitud_db, np.angle(X)


def energia_por_bandas(x, fs, bordes):
    # Reparte la energia del espectro entre las bandas que delimitan los bordes
    X = np.fft.rfft(np.asarray(x, dtype=float))
    frecuencias = np.fft.rfftfreq(np.asarray(x).size, 1/fs)
    densidad = np.abs(X)**2
    total = densidad.sum() + EPSILON
    reparto = []
    for inferior, superior in zip(bordes[:-1], bordes[1:]):
        dentro = (frecuencias >= inferior) & (frecuencias < superior)
        reparto.append({'banda_hz': (float(inferior), float(superior)),
                        'energia': float(densidad[dentro].sum()),
                        'fraccion': float(densidad[dentro].sum()/total)})
    return reparto


def energia_en_banda(x, fs, banda):
    # Energia espectral contenida entre dos frecuencias
    inferior, superior = banda
    X = np.fft.rfft(np.asarray(x, dtype=float))
    frecuencias = np.fft.rfftfreq(np.asarray(x).size, 1/fs)
    dentro = (frecuencias >= inferior) & (frecuencias <= superior)
    return float((np.abs(X[dentro])**2).sum())


def energia_util_conservada(limpia, estimada, fs, bandas_utiles):
    # Fraccion de la energia de la senal util que sobrevive al filtrado
    antes = sum(energia_en_banda(limpia, fs, b) for b in bandas_utiles)
    despues = sum(energia_en_banda(estimada, fs, b) for b in bandas_utiles)
    return float(despues/(antes + EPSILON))


def atenuacion_db(entrada, salida, fs, banda):
    # Cuanto bajo la energia de una banda entre la entrada y la salida
    antes = energia_en_banda(entrada, fs, banda)
    despues = energia_en_banda(salida, fs, banda)
    return float(10*np.log10((antes + EPSILON)/(despues + EPSILON)))


def bandas_utiles_de(frecuencias, ancho=60.0):
    # Una ventana estrecha alrededor de cada tono util
    return [(max(0.0, f - ancho), f + ancho) for f in frecuencias]

# Reduccion espectral FFT/IFFT
# Punto 2.4

from pathlib import Path

import numpy as np

import senales
from dft_fft import fft, ifft

RAIZ = Path(__file__).resolve().parent.parent
CARPETA_SALIDAS = RAIZ / "resultados" / "salidas"

FRECUENCIAS_UTILES = [440, 880, 1320]


def planitud_espectral(magnitud):
    magnitud = np.maximum(magnitud, 1e-12)
    return float(
        np.exp(np.mean(np.log(magnitud))) /
        np.mean(magnitud)
    )


def bins_utiles(n_fft, fs):
    return [
        int(round(f * n_fft / fs))
        for f in FRECUENCIAS_UTILES
    ]


def clasificar_ruido(magnitud):

    mitad = magnitud[:len(magnitud)//2]

    flatness = planitud_espectral(mitad)

    ratio = (
        np.max(mitad) /
        (np.median(mitad) + 1e-12)
    )

    ratio_db = 20 * np.log10(ratio)

    if ratio_db > 40:
        return "tonal"

    if flatness > 0.10:
        return "blanco"

    return "banda"


def conservar_simetria(X):

    N = len(X)

    for k in range(1, N // 2):
        X[-k] = np.conj(X[k])

    return X


def filtrar_tonal(X, fs):

    N = len(X)

    magnitud = np.abs(X)

    candidata = magnitud.copy()

    for b in bins_utiles(N, fs):

        candidata[max(0, b - 2):b + 3] = 0

    k = np.argmax(candidata[:N // 2])

    ancho = 2

    for i in range(
        max(1, k - ancho),
        min(N // 2, k + ancho + 1)
    ):
        X[i] *= 0.05

    return conservar_simetria(X)


def filtrar_blanco(X):

    magnitud = np.abs(X)
    fase = np.angle(X)

    ruido = np.median(magnitud)

    ganancia = np.maximum(
        0.20,
        1.0 - ruido / (magnitud + 1e-12)
    )

    Y = magnitud * ganancia * np.exp(1j * fase)

    return conservar_simetria(Y)


def filtrar_banda(X, fs):

    N = len(X)

    frecuencias = np.arange(N) * fs / N

    banda = (
        (frecuencias >= 800)
        &
        (frecuencias <= 1500)
    )

    Y = X.copy()

    Y[banda] *= 0.20

    return conservar_simetria(Y)


def procesar_bloque(X, fs):

    tipo = clasificar_ruido(np.abs(X))

    if tipo == "tonal":
        return filtrar_tonal(X.copy(), fs)

    if tipo == "blanco":
        return filtrar_blanco(X.copy())

    return filtrar_banda(X.copy(), fs)

def reducir_ruido(
        x,
        fs,
        n_bloque=1024,
        solapamiento=0.5):

    hop = int(
        n_bloque * (1.0 - solapamiento)
    )

    ventana = np.hanning(n_bloque)

    salida = np.zeros(len(x) + n_bloque)

    normalizacion = np.zeros(
        len(x) + n_bloque
    )

    for inicio in range(
            0,
            len(x) - n_bloque + 1,
            hop):

        bloque = (
            x[inicio:inicio+n_bloque]
            * ventana
        )

        X = fft(bloque)

        Y = procesar_bloque(X, fs)

        reconstruida = np.real(ifft(Y))

        salida[
            inicio:inicio+n_bloque
        ] += reconstruida * ventana

        normalizacion[
            inicio:inicio+n_bloque
        ] += ventana ** 2

    validos = normalizacion > 1e-12

    salida[validos] /= normalizacion[validos]

    return salida[:len(x)]


def procesar_caso(identificador):

    datos = senales.cargar_caso(
        identificador
    )

    salida = reducir_ruido(
        datos["contaminada"],
        datos["fs"]
    )

    CARPETA_SALIDAS.mkdir(
        parents=True,
        exist_ok=True
    )

    senales.escribir_wav(
        CARPETA_SALIDAS /
        f"{identificador}_espectral.wav",
        salida,
        datos["fs"]
    )

    return salida


def generar_todos():

    for identificador in (
            "caso_1",
            "caso_2",
            "caso_3"):

        procesar_caso(
            identificador
        )

        print(
            f"procesado {identificador}"
        )


if __name__ == "__main__":
    generar_todos()
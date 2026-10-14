# Reduccion espectral por FFT/IFFT
# Punto 2.4

from pathlib import Path

import numpy as np

from dft_fft import fft, ifft
import senales


RAIZ = Path(__file__).resolve().parent.parent
CARPETA_SALIDAS = RAIZ / "resultados" / "salidas"


def planitud_espectral(magnitud):
    magnitud = np.maximum(magnitud, 1e-12)
    return np.exp(np.mean(np.log(magnitud))) / np.mean(magnitud)


def clasificar_ruido(magnitud):
    mitad = magnitud[:len(magnitud) // 2]

    flatness = planitud_espectral(mitad)

    ratio_db = 20 * np.log10(
        np.max(mitad) /
        (np.median(mitad) + 1e-12)
    )

    if ratio_db > 40:
        return "tonal"

    if flatness > 0.1:
        return "blanco"

    return "banda"


def filtrado_bloque(X):

    magnitud = np.abs(X)
    fase = np.angle(X)

    tipo = clasificar_ruido(magnitud)

    Y = X.copy()

    if tipo == "tonal":

        k = np.argmax(magnitud[1:len(magnitud)//2]) + 1

        ancho = 2

        for i in range(
                max(1, k - ancho),
                min(len(magnitud)//2, k + ancho + 1)):
            Y[i] *= 0.05
            Y[-i] = np.conj(Y[i])

    elif tipo == "blanco":

        ruido = np.median(magnitud)

        ganancia = np.maximum(
            0.2,
            1.0 - ruido/(magnitud + 1e-12)
        )

        Y = magnitud * ganancia * np.exp(1j * fase)

    else:

        mediana = np.median(magnitud)

        umbral = 2 * mediana

        bins = np.where(
            magnitud[:len(magnitud)//2] > umbral
        )[0]

        if bins.size:

            inicio = bins[0]
            fin = bins[-1]

            Y[inicio:fin+1] *= 0.15

            for k in range(inicio, fin + 1):
                Y[-k] = np.conj(Y[k])

    return Y


def reducir_ruido(
        x,
        n_bloque=1024,
        solapamiento=0.5):

    hop = int(n_bloque * (1 - solapamiento))

    ventana = np.hanning(n_bloque)

    salida = np.zeros(len(x) + n_bloque)

    normalizacion = np.zeros(len(x) + n_bloque)

    for inicio in range(
            0,
            len(x) - n_bloque + 1,
            hop):

        bloque = x[
            inicio:inicio+n_bloque
        ] * ventana

        X = fft(bloque)

        Y = filtrado_bloque(X)

        reconstruida = np.real(ifft(Y))

        salida[
            inicio:inicio+n_bloque
        ] += reconstruida * ventana

        normalizacion[
            inicio:inicio+n_bloque
        ] += ventana**2

    indices = normalizacion > 1e-12

    salida[indices] /= normalizacion[indices]

    return salida[:len(x)]


def procesar_caso(identificador):

    datos = senales.cargar_caso(identificador)

    salida = reducir_ruido(
        datos["contaminada"],
        n_bloque=1024,
        solapamiento=0.5
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

        procesar_caso(identificador)

        print(
            f"Procesado {identificador}"
        )


if __name__ == "__main__":
    generar_todos()
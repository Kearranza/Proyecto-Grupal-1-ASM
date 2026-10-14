from pathlib import Path

import numpy as np

from matplotlib.figure import Figure

import metricas
import senales
import filtrado_espectral

RAIZ = Path(__file__).resolve().parent.parent

CARPETA_FIGURAS = (
    RAIZ /
    "resultados" /
    "figuras"
)


def generar_figura_caso(
        identificador):

    datos = senales.cargar_caso(
        identificador
    )

    salida = (
        filtrado_espectral
        .reducir_ruido(
            datos["contaminada"],
            datos["fs"]
        )
    )

    fig = Figure(
        figsize=(12, 7)
    )

    ax = fig.subplots(2, 2)

    #
    # PANEL 1
    #

    inicio = datos["fs"]

    fin = inicio + int(
        0.025 * datos["fs"]
    )

    tiempo = datos["t"][inicio:fin] * 1000

    ax[0,0].plot(
        tiempo,
        datos["contaminada"][inicio:fin],
        label="Contaminada"
    )

    ax[0,0].plot(
        tiempo,
        salida[inicio:fin],
        label="Filtrada"
    )

    ax[0,0].set_title(
        "Dominio temporal"
    )

    ax[0,0].set_xlabel(
        "Tiempo (ms)"
    )

    ax[0,0].legend()

    #
    # PANEL 2
    #

    f1,m1,_ = metricas.espectro(
        datos["contaminada"],
        datos["fs"]
    )

    f2,m2,_ = metricas.espectro(
        salida,
        datos["fs"]
    )

    ax[0,1].plot(
        f1,m1,
        label="Contaminada"
    )

    ax[0,1].plot(
        f2,m2,
        label="Filtrada"
    )

    ax[0,1].set_title(
        "Espectro completo"
    )

    ax[0,1].legend()

    #
    # PANEL 3
    #

    if identificador == "caso_1":

        zoom = (
            (f1 >= 3200)
            &
            (f1 <= 3800)
        )

    elif identificador == "caso_3":

        zoom = (
            (f1 >= 700)
            &
            (f1 <= 1600)
        )

    else:

        zoom = (
            (f1 >= 0)
            &
            (f1 <= 2500)
        )

    ax[1,0].plot(
        f1[zoom],
        m1[zoom]
    )

    ax[1,0].plot(
        f2[zoom],
        m2[zoom]
    )

    ax[1,0].set_title(
        "Zoom región relevante"
    )

    #
    # PANEL 4
    #

    diferencia = m1 - m2

    ax[1,1].plot(
        f1,
        diferencia
    )

    ax[1,1].set_title(
        "Atenuación aplicada"
    )

    ax[1,1].set_xlabel(
        "Frecuencia (Hz)"
    )

    ax[1,1].set_ylabel(
        "Δ Magnitud (dB)"
    )

    fig.suptitle(
        identificador
    )

    ruta = (
        CARPETA_FIGURAS /
        f"fig_24_{identificador}.png"
    )

    fig.tight_layout()

    fig.savefig(
        ruta,
        dpi=160
    )

    return ruta

def generar_figura_compromiso():

    datos = senales.cargar_caso(
        "caso_3"
    )

    entrada = datos["contaminada"]

    fs = datos["fs"]

    salida = (
        filtrado_espectral
        .reducir_ruido(
            entrada,
            fs
        )
    )

    f0,m0,_ = metricas.espectro(
        entrada,
        fs
    )

    f1,m1,_ = metricas.espectro(
        salida,
        fs
    )

    fig = Figure(
        figsize=(10,4)
    )

    ax = fig.subplots()

    zoom = (
        (f0 >= 700)
        &
        (f0 <= 1600)
    )

    ax.plot(
        f0[zoom],
        m0[zoom],
        label="Entrada"
    )

    ax.plot(
        f1[zoom],
        m1[zoom],
        label="Filtrada"
    )

    ax.set_title(
        "Caso 3: compromiso ruido/distorsión"
    )

    ax.legend()

    ruta = (
        CARPETA_FIGURAS /
        "fig_24_compromiso.png"
    )

    fig.savefig(
        ruta,
        dpi=160
    )

    return ruta

def generar_compromiso():

    datos = senales.cargar_caso(
        "caso_3"
    )

    entrada = datos["contaminada"]

    fs = datos["fs"]

    f, m0, _ = metricas.espectro(
        entrada,
        fs
    )

    fig = Figure(
        figsize=(10, 4)
    )

    ax = fig.subplots()

    ax.plot(
        f,
        m0,
        label="Entrada"
    )

    salida = (
        filtrado_espectral
        .reducir_ruido(
            entrada,
            fs
        )
    )

    f2, m2, _ = metricas.espectro(
        salida,
        fs
    )

    ax.plot(
        f2,
        m2,
        label="Salida"
    )

    ax.set_title(
        "Compromiso ruido/distorsion"
    )

    ax.set_xlabel(
        "Frecuencia (Hz)"
    )

    ax.set_ylabel(
        "Magnitud (dB)"
    )

    ax.legend()

    ruta = (
        CARPETA_FIGURAS /
        "fig_24_compromiso.png"
    )

    fig.savefig(
        ruta,
        dpi=160
    )

    return ruta


def generar_todas():

    CARPETA_FIGURAS.mkdir(
        parents=True,
        exist_ok=True
    )

    for caso in (
        "caso_1",
        "caso_2",
        "caso_3"
    ):
        generar_figura_caso(caso)

    generar_compromiso()
    generar_figura_compromiso()


if __name__ == "__main__":
    generar_todas()
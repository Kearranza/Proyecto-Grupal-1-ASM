# Figuras del punto 2.2: caracterizacion temporal y espectral de los tres casos

from pathlib import Path

import numpy as np
from matplotlib.figure import Figure

import metricas
import senales

LIMPIA = '#2a78d6'
CONTAMINADA = '#eb6834'
RUIDO = '#1baf7a'
TINTA = '#0b0b0b'
TINTA_SUAVE = '#52514e'
APAGADO = '#898781'
REJILLA = '#e1e0d9'
EJE = '#c3c2b7'
SUPERFICIE = '#fcfcfb'

BORDES_DE_BANDA = [0, 250, 500, 1000, 2000, 4000, 8000]
CARPETA_FIGURAS = senales.RAIZ / 'resultados' / 'figuras'


def _estilo(ax):
    ax.set_facecolor(SUPERFICIE)
    ax.grid(True, color=REJILLA, linewidth=0.8)
    ax.set_axisbelow(True)
    for lado in ('top', 'right'):
        ax.spines[lado].set_visible(False)
    for lado in ('left', 'bottom'):
        ax.spines[lado].set_color(EJE)
    ax.tick_params(colors=APAGADO, labelsize=8)
    ax.xaxis.label.set_color(TINTA_SUAVE)
    ax.yaxis.label.set_color(TINTA_SUAVE)
    ax.title.set_color(TINTA)


def _panel_temporal(ax, datos, milisegundos=25, desde_s=1.0):
    # Una ventana del centro, no del inicio, para no mostrar solo el desvanecido
    inicio = int(datos['fs']*desde_s)
    fin = inicio + int(datos['fs']*milisegundos/1000)
    t = datos['t'][inicio:fin]*1000
    ax.plot(t, datos['contaminada'][inicio:fin], color=CONTAMINADA, linewidth=1.2,
            label='Contaminada')
    ax.plot(t, datos['limpia'][inicio:fin], color=LIMPIA, linewidth=2.0, label='Limpia')
    ax.set(xlabel='Tiempo (ms)', ylabel='Amplitud',
           title=f'Señal en el tiempo ({milisegundos} ms desde {desde_s:.0f} s)')
    ax.legend(frameon=False, fontsize=8, labelcolor=TINTA_SUAVE)
    _estilo(ax)


def _panel_magnitud(ax, datos):
    f, mag_limpia, _ = metricas.espectro(datos['limpia'], datos['fs'])
    _, mag_sucia, _ = metricas.espectro(datos['contaminada'], datos['fs'])
    ax.plot(f, mag_sucia, color=CONTAMINADA, linewidth=1.0, label='Contaminada')
    ax.plot(f, mag_limpia, color=LIMPIA, linewidth=1.4, label='Limpia')
    for frecuencia in datos['config']['frecuencias_utiles_hz']:
        ax.axvline(frecuencia, color=APAGADO, linewidth=0.8, linestyle=':')
    ax.set(xlabel='Frecuencia (Hz)', ylabel='Magnitud (dB)',
           title='Espectro de magnitud (ventana Hann)',
           xlim=(0, datos['fs']/2), ylim=(mag_sucia.max()-110, mag_sucia.max()+8))
    ax.legend(frameon=False, fontsize=8, labelcolor=TINTA_SUAVE)
    _estilo(ax)


def _panel_fase(ax, datos):
    # Solo los bines con energia significativa: donde domina el ruido la fase es azar
    f, mag_limpia, fase_limpia = metricas.espectro(datos['limpia'], datos['fs'])
    _, mag_sucia, fase_sucia = metricas.espectro(datos['contaminada'], datos['fs'])
    umbral_sucia = mag_sucia.max() - 60
    umbral_limpia = mag_limpia.max() - 60
    ax.scatter(f[mag_sucia > umbral_sucia], fase_sucia[mag_sucia > umbral_sucia],
               s=70, color=CONTAMINADA, label='Contaminada', zorder=2)
    ax.scatter(f[mag_limpia > umbral_limpia], fase_limpia[mag_limpia > umbral_limpia],
               s=18, color=LIMPIA, edgecolor=SUPERFICIE, linewidth=0.8,
               label='Limpia', zorder=3)
    ax.set(xlabel='Frecuencia (Hz)', ylabel='Fase (rad)',
           title='Fase en los bines a menos de 60 dB del máximo',
           xlim=(0, datos['fs']/2), ylim=(-np.pi*1.1, np.pi*1.1))
    ax.set_yticks([-np.pi, 0, np.pi])
    ax.set_yticklabels(['-π', '0', 'π'])
    ax.legend(frameon=False, fontsize=8, labelcolor=TINTA_SUAVE)
    _estilo(ax)


def _panel_bandas(ax, datos):
    reparto_limpia = metricas.energia_por_bandas(datos['limpia'], datos['fs'], BORDES_DE_BANDA)
    reparto_sucia = metricas.energia_por_bandas(datos['contaminada'], datos['fs'], BORDES_DE_BANDA)
    etiquetas = [f"{int(r['banda_hz'][0])}–\n{int(r['banda_hz'][1])}" for r in reparto_limpia]
    posicion = np.arange(len(etiquetas))
    ancho = 0.38
    ax.bar(posicion - ancho/2, [100*r['fraccion'] for r in reparto_limpia],
           ancho*0.94, color=LIMPIA, label='Limpia')
    ax.bar(posicion + ancho/2, [100*r['fraccion'] for r in reparto_sucia],
           ancho*0.94, color=CONTAMINADA, label='Contaminada')
    ax.set_xticks(posicion)
    ax.set_xticklabels(etiquetas, fontsize=7)
    ax.set(xlabel='Banda (Hz)', ylabel='Energía (%)',
           title='Distribución de energía por bandas')
    ax.legend(frameon=False, fontsize=8, labelcolor=TINTA_SUAVE)
    _estilo(ax)


def figura_de_caso(datos, carpeta=None):
    carpeta = Path(carpeta) if carpeta else CARPETA_FIGURAS
    carpeta.mkdir(parents=True, exist_ok=True)
    fig = Figure(figsize=(11.5, 7.2), facecolor=SUPERFICIE)
    ejes = fig.subplots(2, 2)
    _panel_temporal(ejes[0, 0], datos)
    _panel_magnitud(ejes[0, 1], datos)
    _panel_fase(ejes[1, 0], datos)
    _panel_bandas(ejes[1, 1], datos)
    fig.suptitle(f"{datos['id']} — {datos['config']['nombre']}   "
                 f"(SNR {datos['config']['snr_objetivo_db']:.0f} dB)",
                 color=TINTA, fontsize=12, fontweight='bold')
    fig.tight_layout(rect=(0, 0, 1, 0.96))
    ruta = carpeta / f"fig_22_{datos['id']}.png"
    fig.savefig(ruta, dpi=160, facecolor=SUPERFICIE)
    return ruta


def figura_comparativa(todos, carpeta=None):
    # Los tres espectros contaminados juntos, con la banda util sombreada
    carpeta = Path(carpeta) if carpeta else CARPETA_FIGURAS
    carpeta.mkdir(parents=True, exist_ok=True)
    fig = Figure(figsize=(13, 3.6), facecolor=SUPERFICIE)
    ejes = fig.subplots(1, 3, sharey=True)
    for ax, datos in zip(ejes, todos):
        f, mag, _ = metricas.espectro(datos['contaminada'], datos['fs'])
        utiles = datos['config']['frecuencias_utiles_hz']
        ax.axvspan(min(utiles)-150, max(utiles)+150, color=LIMPIA, alpha=0.10,
                   linewidth=0)
        ax.plot(f, mag, color=CONTAMINADA, linewidth=1.0)
        ax.set(xlabel='Frecuencia (Hz)', xlim=(0, 5000),
               ylim=(mag.max()-100, mag.max()+8),
               title=f"{datos['id']}: ruido {datos['config']['tipo_ruido']}")
        _estilo(ax)
    ejes[0].set_ylabel('Magnitud (dB)')
    fig.suptitle('Señal contaminada en los tres casos. El sombreado marca la banda útil',
                 color=TINTA, fontsize=11)
    fig.tight_layout(rect=(0, 0, 1, 0.9))
    ruta = carpeta / 'fig_22_comparacion.png'
    fig.savefig(ruta, dpi=160, facecolor=SUPERFICIE)
    return ruta


def generar_todas(verbose=True):
    _, casos = senales.cargar_configuracion()
    todos = [senales.cargar_caso(identificador) for identificador in casos]
    rutas = [figura_de_caso(datos) for datos in todos]
    rutas.append(figura_comparativa(todos))
    if verbose:
        for ruta in rutas:
            print('escrita', ruta.name)
    return rutas


if __name__ == '__main__':
    generar_todas()

"""BLOQUE B - Sistema completo en alto nivel (rama: taller-sistema)

Punto 1 del taller: simular de punta a punta lo que despues correra en los
micros, con las mismas frecuencias del emisor. Reutiliza la Tarea 1:
  src/filtros_temporales.py  -> nodo 2 (FIR/IIR)
  src/filtrado_espectral.py  -> nodo 3 (FFT/IFFT por bloques)
  src/metricas.py            -> MSE, SNR, energia, atenuacion

Flujo que debe quedar (una funcion por nodo, igual que la arquitectura fisica):

  emisor (A) -> cadena fisica -> nodo 1 -> nodo 2 (temporal) -+-> metricas
                PWM 8b + RC      ADC+FFT   nodo 3 (espectral) -+
                + ADC 12b        +diagnostico

Correr desde la raiz del repo:
    python taller_semana12/b_sistema/sistema_completo.py

Entrega de B en el PDF (informe/secciones/2_sistema.tex):
  1. Diagrama de bloques del sistema simulado y que modela cada funcion.
  2. Para cada modo: limpia vs contaminada vs las dos salidas (tiempo y espectro).
  3. Tabla de metricas por modo y metodo.
  4. Estimacion de costo: operaciones, memoria y latencia por bloque de cada metodo
     (seccion 3.2.7 del proyecto).
"""
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comun import parametros as P  # noqa: E402
from comun.pasos import ejecutar  # noqa: E402

P.usar_src()


def entrada(p, identificador):
    """Senal limpia y contaminada del modo. Viene del bloque A.

    Mientras A no termine, se puede usar senales.generar_caso('caso_1') de la
    Tarea 1 para avanzar.
    """
    # TODO (B): from a_senales.prototipo_frecuencias import senal_del_modo
    raise NotImplementedError('TODO (B): conectar con senal_del_modo() de A')


def cadena_fisica(x, p):
    """Modelo de lo que le pasa a la senal entre el emisor (Pico) y el receptor (Pico W).

    Minimo: cuantizar a bits_salida, (opcional) filtro RC de reconstruccion,
    offset de media escala y cuantizar a bits_adc. Asi la simulacion ya trae el
    ruido de cuantizacion que vera el sistema real.
    """
    raise NotImplementedError('TODO (B): modelo PWM -> RC -> ADC')


def nodo_1_adquisicion(x, p):
    """Quitar DC, ventana, FFT y diagnostico del ruido (tipo y frecuencia).

    Puede usar filtrado_espectral.clasificar_ruido(). Devuelve lo que se le
    mandaria a los nodos 2 y 3 (los parametros, no la senal entera).
    """
    raise NotImplementedError('TODO (B): nodo 1')


def nodo_2_temporal(x, diagnostico, p):
    """Filtro FIR/IIR elegido a partir del diagnostico. Usar filtros_temporales."""
    raise NotImplementedError('TODO (B): nodo 2')


def nodo_3_espectral(x, diagnostico, p):
    """Reduccion por bloques. Usar filtrado_espectral.reducir_ruido()."""
    raise NotImplementedError('TODO (B): nodo 3')


def evaluar(limpia, contaminada, salidas, p):
    """MSE, SNR entrada/salida, delta SNR, energia util y atenuacion (metricas.py)."""
    raise NotImplementedError('TODO (B): metricas')


def estimar_costo(p):
    """Operaciones, memoria (bytes) y latencia por bloque de cada metodo."""
    raise NotImplementedError('TODO (B): costo computacional')


def correr_modo(p, identificador):
    limpia, contaminada = entrada(p, identificador)
    medida = cadena_fisica(contaminada, p)
    diagnostico = nodo_1_adquisicion(medida, p)
    salidas = {
        'temporal': nodo_2_temporal(medida, diagnostico, p),
        'espectral': nodo_3_espectral(medida, diagnostico, p),
    }
    return evaluar(limpia, medida, salidas, p)


def main():
    p = P.cargar()
    pasos = [(f'modo {m["id"]} de punta a punta', lambda i=m['id']: correr_modo(p, i))
             for m in p['emisor']['modos']]
    pasos.append(('costo computacional', lambda: estimar_costo(p)))
    ejecutar('Bloque B - sistema completo', pasos)


if __name__ == '__main__':
    main()

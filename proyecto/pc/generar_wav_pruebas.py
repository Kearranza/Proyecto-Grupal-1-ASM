"""Senales de las pruebas P3-P5 para reproducir desde la computadora (Santiago).

El enunciado permite ruido "generado o reproducido desde un archivo". Se generan a
FS_HZ y 16 bits, con la SNR controlada, extendiendo src/senales.py de la Tarea 1
(no se crea otro generador):
  P3: audio o senal arbitraria + ruido de banda ancha
  P4: senal transitoria (p. ej. rafaga o escalon suavizado) + ruido impulsivo
  P5: senal con ruido de banda que se superpone parcialmente (como caso_3)
Ademas, al menos tres tipos de ruido de la seccion 2.2: tambien 60 Hz con armonicos.

    python proyecto/pc/generar_wav_pruebas.py

Guarda en proyecto/senales/ la limpia, el ruido y la contaminada de cada prueba,
porque la limpia es la referencia de las metricas.
"""
import sys
from pathlib import Path

PROYECTO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROYECTO.parent / 'src'))
sys.path.insert(0, str(PROYECTO / 'pc'))
from pasos import ejecutar  # noqa: E402


def ruido_impulsivo():
    raise NotImplementedError('TODO (Santiago): agregar a src/senales.py')


def interferencia_60hz():
    raise NotImplementedError('TODO (Santiago): 60 Hz + armonicos en src/senales.py')


def generar(prueba):
    raise NotImplementedError(f'TODO (Santiago): {prueba}')


def main():
    ejecutar('Senales de prueba', [
        ('ruido impulsivo', ruido_impulsivo),
        ('60 Hz con armonicos', interferencia_60hz),
        ('P3', lambda: generar('P3')),
        ('P4', lambda: generar('P4')),
        ('P5', lambda: generar('P5')),
    ])


if __name__ == '__main__':
    main()

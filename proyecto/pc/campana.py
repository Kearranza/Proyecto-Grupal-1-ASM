"""Campana de pruebas P1-P5 con repeticiones (Kevin).

Lee config/pruebas.json, y por cada prueba y repeticion usa lo registrado con
registrar.py para calcular las metricas con src/metricas.py (Tarea 1). Esa es la
validacion independiente en la computadora; no se reescriben las metricas.

    python proyecto/pc/campana.py

Salida: resultados/campana.csv (promedio y desviacion por prueba y metodo) y las
figuras resultados/figuras/p*_*.png para el paper.
"""
import json
import sys
from pathlib import Path

PROYECTO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROYECTO.parent / 'src'))
sys.path.insert(0, str(PROYECTO / 'pc'))
from pasos import ejecutar  # noqa: E402

PRUEBAS = json.loads((PROYECTO / 'config' / 'pruebas.json').read_text(encoding='utf-8'))


def verificar_criterios():
    faltan = [k for k, v in PRUEBAS['criterios_aceptacion'].items()
              if not k.startswith('_') and v is None]
    if faltan:
        raise NotImplementedError(f'TODO (Kevin): fijar antes de medir {faltan}')


def medir_prueba(prueba):
    raise NotImplementedError(f'TODO (Kevin): {prueba["id"]} x {PRUEBAS["repeticiones"]}')


def resumen():
    raise NotImplementedError('TODO (Kevin): promedio y desviacion, campana.csv y figuras')


def main():
    pasos = [('criterios fijados antes de medir', verificar_criterios)]
    pasos += [(f'{p["id"]}: {p["senal"]}', lambda p=p: medir_prueba(p))
              for p in PRUEBAS['pruebas'] if p['id'] != 'P6']
    pasos.append(('resumen y figuras', resumen))
    ejecutar('Campana de pruebas', pasos)


if __name__ == '__main__':
    main()

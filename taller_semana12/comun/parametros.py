"""Carga unica de parametros del taller (compartido por los cuatro bloques).

Junta en un solo diccionario:
  - lo comun de la Tarea 1 (fs, N, ventana, casos) -> metadatos/configuraciones.json
  - lo nuevo del taller (emisor, receptor, validacion) -> taller_semana12/parametros.json

Regla del grupo: ningun script escribe 16000, 1024 o 440 a mano. Si en la defensa
cambian un parametro, se edita el JSON y todo lo demas lo sigue.

Uso desde cualquier carpeta del taller:
    from comun import parametros as P
    p = P.cargar()
    P.usar_src()          # permite importar los modulos de src/ de la Tarea 1
    import senales, metricas
"""
import json
import sys
from pathlib import Path

TALLER = Path(__file__).resolve().parent.parent
RAIZ = TALLER.parent
ARCHIVO_TALLER = TALLER / 'parametros.json'
ARCHIVO_TAREA1 = RAIZ / 'metadatos' / 'configuraciones.json'
CARPETA_FIGURAS = TALLER / 'resultados' / 'figuras'
CARPETA_RESULTADOS = TALLER / 'resultados'


def usar_src():
    # Los modulos de la Tarea 1 viven en src/ y no son un paquete instalado
    ruta = str(RAIZ / 'src')
    if ruta not in sys.path:
        sys.path.insert(0, ruta)


def cargar():
    taller = json.loads(ARCHIVO_TALLER.read_text(encoding='utf-8'))
    tarea1 = json.loads(ARCHIVO_TAREA1.read_text(encoding='utf-8'))
    comun = tarea1['comun']

    emisor = dict(taller['emisor'])
    if emisor.get('fs_hz') is None:
        emisor['fs_hz'] = comun['fs_hz']

    return {
        'fs_hz': comun['fs_hz'],
        'n_bloque': comun['n_bloque'],
        'solapamiento': comun['solapamiento'],
        'ventana': comun['ventana'],
        'casos': {c['id']: c for c in tarea1['casos']},
        'emisor': emisor,
        'receptor': taller['receptor'],
        'validacion': taller['validacion'],
    }


def modo(p, identificador):
    for m in p['emisor']['modos']:
        if m['id'] == identificador:
            return m
    raise KeyError(f'No existe el modo {identificador} en parametros.json')


def resolucion_hz(p):
    return p['fs_hz'] / p['n_bloque']


if __name__ == '__main__':
    p = cargar()
    print(f"fs = {p['fs_hz']} Hz | N = {p['n_bloque']} | "
          f"resolucion = {resolucion_hz(p):.3f} Hz | ventana {p['ventana']}")
    for m in p['emisor']['modos']:
        print(f"  modo {m['id']}: {m['nombre']} -> {m['frecuencias_hz']} Hz")

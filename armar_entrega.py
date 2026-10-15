# Arma el .zip de entrega con la estructura que pide el enunciado
#
#   python armar_entrega.py 01   ->  tarea1_grupo01.zip

import csv
import shutil
import sys
import zipfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parent

# (origen, destino dentro del zip). Un directorio se copia completo.
CONTENIDO = [
    ('informe/informe.pdf', 'informe.pdf'),
    ('notebook.ipynb', 'notebook.ipynb'),
    ('README.md', 'README.md'),
    ('src', 'src'),
    ('pruebas', 'pruebas'),
    ('resultados', 'resultados'),
    ('metadatos', 'metadatos'),
    ('informe', 'informe'),
    ('docs', 'docs'),
]

EXCLUIR = {'__pycache__', '.ipynb_checkpoints', '.git'}
EXTENSIONES_EXCLUIDAS = {'.pyc', '.aux', '.log', '.out', '.toc', '.bbl', '.blg',
                         '.fls', '.fdb_latexmk', '.synctex.gz'}


def revisar():
    # Comprueba lo que el enunciado exige antes de empaquetar
    problemas = []

    if not (RAIZ / 'informe' / 'informe.pdf').exists():
        problemas.append('falta informe/informe.pdf: compilar el informe antes de armar el zip')

    salidas = sorted((RAIZ / 'resultados' / 'salidas').glob('*.wav'))
    if len(salidas) != 6:
        problemas.append(f'se esperaban 6 salidas filtradas y hay {len(salidas)}')

    for caso in ('caso_1', 'caso_2', 'caso_3'):
        carpeta = RAIZ / 'pruebas' / caso
        for nombre in ('limpia.wav', 'ruido.wav', 'contaminada.wav', f'{caso}.json'):
            if not (carpeta / nombre).exists():
                problemas.append(f'falta pruebas/{caso}/{nombre}')

    metricas = RAIZ / 'resultados' / 'metricas.csv'
    if not metricas.exists():
        problemas.append('falta resultados/metricas.csv: correr src/comparacion.py')
    else:
        with open(metricas, encoding='utf-8') as archivo:
            filas = list(csv.DictReader(archivo))
        if len(filas) != 9:
            problemas.append(f'metricas.csv tiene {len(filas)} filas y deberia tener 9')
        elif any(f['metodo'] == 'sin filtrar' for f in filas) and \
                not any(f['metodo'] == 'espectral' for f in filas):
            problemas.append('metricas.csv solo trae la linea de base')

    pendientes = [p.name for p in (RAIZ / 'informe' / 'secciones').glob('*.tex')
                  if 'pendiente' in p.read_text(encoding='utf-8')]
    if pendientes:
        problemas.append('secciones del informe sin terminar: ' + ', '.join(pendientes))

    return problemas


def se_incluye(ruta):
    if any(parte in EXCLUIR for parte in ruta.parts):
        return False
    return ''.join(ruta.suffixes[-1:]) not in EXTENSIONES_EXCLUIDAS


def armar(grupo):
    nombre = f'tarea1_grupo{grupo}'
    destino = RAIZ / nombre
    if destino.exists():
        shutil.rmtree(destino)
    destino.mkdir()

    for origen_rel, destino_rel in CONTENIDO:
        origen = RAIZ / origen_rel
        if not origen.exists():
            print(f'  aviso: no existe {origen_rel}, se omite')
            continue
        if origen.is_dir():
            shutil.copytree(origen, destino / destino_rel,
                            ignore=shutil.ignore_patterns(*EXCLUIR, '*.pyc'))
        else:
            shutil.copy2(origen, destino / destino_rel)

    ruta_zip = RAIZ / f'{nombre}.zip'
    if ruta_zip.exists():
        ruta_zip.unlink()
    with zipfile.ZipFile(ruta_zip, 'w', zipfile.ZIP_DEFLATED) as archivo:
        for ruta in sorted(destino.rglob('*')):
            if ruta.is_file() and se_incluye(ruta.relative_to(destino)):
                archivo.write(ruta, nombre + '/' + str(ruta.relative_to(destino)).replace('\\', '/'))

    shutil.rmtree(destino)
    return ruta_zip


def main():
    grupo = sys.argv[1] if len(sys.argv) > 1 else '01'

    problemas = revisar()
    if problemas:
        print('Revision previa:')
        for problema in problemas:
            print('  -', problema)
        print()
        if '--forzar' not in sys.argv:
            print('No se armo el zip. Corregir lo anterior o volver a correr con --forzar.')
            return 1

    ruta = armar(grupo)
    with zipfile.ZipFile(ruta) as archivo:
        total = len(archivo.namelist())
    print(f'{ruta.name}: {total} archivos, {ruta.stat().st_size/1024/1024:.1f} MB')
    return 0


if __name__ == '__main__':
    sys.exit(main())

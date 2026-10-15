# Pruebas automaticas de la generacion de los tres casos (punto 2.2)

import sys
from pathlib import Path

import numpy as np

RAIZ = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(RAIZ / 'src'))

import metricas
import senales

COMUN, CASOS = senales.cargar_configuracion()
TOLERANCIA_SNR_DB = 1e-9
TOLERANCIA_CUANTIZACION_DB = 0.01


def test_la_misma_semilla_da_la_misma_senal():
    for identificador in CASOS:
        a = senales.generar_caso(identificador, COMUN, CASOS)
        b = senales.generar_caso(identificador, COMUN, CASOS)
        assert np.array_equal(a['contaminada'], b['contaminada']), identificador


def test_semillas_distintas_dan_ruidos_distintos():
    casos_alterados = {k: dict(v) for k, v in CASOS.items()}
    casos_alterados['caso_2']['semilla'] += 1
    original = senales.generar_caso('caso_2', COMUN, CASOS)
    alterado = senales.generar_caso('caso_2', COMUN, casos_alterados)
    assert not np.allclose(original['ruido'], alterado['ruido'])


def test_la_snr_obtenida_coincide_con_la_configurada():
    for identificador, caso in CASOS.items():
        datos = senales.generar_caso(identificador, COMUN, CASOS)
        obtenida = metricas.snr_db(datos['limpia'], datos['contaminada'])
        assert abs(obtenida - caso['snr_objetivo_db']) < TOLERANCIA_SNR_DB, \
            f'{identificador}: {obtenida} vs {caso["snr_objetivo_db"]}'


def test_la_cuantizacion_a_16_bits_apenas_mueve_la_snr():
    for identificador, caso in CASOS.items():
        datos = senales.cargar_caso(identificador)
        obtenida = metricas.snr_db(datos['limpia'], datos['contaminada'])
        assert abs(obtenida - caso['snr_objetivo_db']) < TOLERANCIA_CUANTIZACION_DB, \
            f'{identificador}: {obtenida} dB en el WAV'


def test_la_contaminada_es_la_suma_de_las_otras_dos():
    for identificador in CASOS:
        datos = senales.generar_caso(identificador, COMUN, CASOS)
        assert np.allclose(datos['contaminada'], datos['limpia'] + datos['ruido'])


def test_ninguna_frecuencia_pasa_nyquist():
    for caso in CASOS.values():
        senales.verificar_nyquist(caso, COMUN['fs_hz'])


def test_no_hay_saturacion():
    for identificador in CASOS:
        datos = senales.cargar_caso(identificador)
        for nombre in ('limpia', 'ruido', 'contaminada'):
            pico = np.max(np.abs(datos[nombre]))
            assert pico <= COMUN['pico_maximo'] + 1e-3, f'{identificador}/{nombre}: {pico}'


def test_la_longitud_es_potencia_de_dos_y_cabe_un_numero_entero_de_bloques():
    n = int(round(COMUN['duracion_s']*COMUN['fs_hz']))
    assert n & (n - 1) == 0, f'{n} no es potencia de dos'
    assert n % COMUN['n_bloque'] == 0, f'{n} no es multiplo de {COMUN["n_bloque"]}'


def test_los_wav_conservan_frecuencia_y_longitud():
    n = int(round(COMUN['duracion_s']*COMUN['fs_hz']))
    for identificador in CASOS:
        datos = senales.cargar_caso(identificador)
        assert datos['fs'] == COMUN['fs_hz']
        assert datos['limpia'].size == n


def test_el_ruido_de_banda_queda_dentro_de_su_banda():
    datos = senales.cargar_caso('caso_3')
    banda = CASOS['caso_3']['banda_ruido_hz']
    dentro = metricas.energia_en_banda(datos['ruido'], datos['fs'],
                                       (banda[0] - 100, banda[1] + 100))
    total = metricas.energia_en_banda(datos['ruido'], datos['fs'], (0, datos['fs']/2))
    assert dentro/total > 0.99, f'solo el {100*dentro/total:.2f} % cae en la banda'


def test_el_ruido_tonal_concentra_su_energia_en_la_interferencia():
    datos = senales.cargar_caso('caso_1')
    frecuencia = CASOS['caso_1']['frecuencia_interferencia_hz']
    dentro = metricas.energia_en_banda(datos['ruido'], datos['fs'],
                                       (frecuencia - 50, frecuencia + 50))
    total = metricas.energia_en_banda(datos['ruido'], datos['fs'], (0, datos['fs']/2))
    assert dentro/total > 0.99, f'solo el {100*dentro/total:.2f} % cae en el tono'


def main():
    pruebas = [v for k, v in sorted(globals().items()) if k.startswith('test_')]
    fallos = 0
    for prueba in pruebas:
        try:
            prueba()
            print(f'  ok   {prueba.__name__}')
        except AssertionError as error:
            fallos += 1
            print(f'  FALLA {prueba.__name__}: {error}')
    print(f'\n{len(pruebas) - fallos}/{len(pruebas)} pruebas pasan')
    return 1 if fallos else 0


if __name__ == '__main__':
    sys.exit(main())

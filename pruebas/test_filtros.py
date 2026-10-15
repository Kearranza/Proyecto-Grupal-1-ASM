# Pruebas automaticas del filtro temporal (punto 2.3)

import sys
from pathlib import Path

import numpy as np

RAIZ = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(RAIZ / 'src'))

import filtros_temporales as ft
import senales

COMUN, CASOS = senales.cargar_configuracion()
FS = float(COMUN['fs_hz'])
F_INT = float(CASOS['caso_1']['frecuencia_interferencia_hz'])
UTILES = [float(f) for f in CASOS['caso_1']['frecuencias_utiles_hz']]

MARGEN_RECHAZO_HZ = 30.0     # semiancho de la banda de rechazo
MARGEN_PASO_HZ = 100.0       # la banda de paso empieza a esta distancia
GPASS_DB, GSTOP_DB = 1.0, 40.0
TOLERANCIA_EQUIVALENCIA = 1e-9
N_TRANSITORIO = 500          # muestras que se descartan al medir

WS = [F_INT - MARGEN_RECHAZO_HZ, F_INT + MARGEN_RECHAZO_HZ]
WP = [F_INT - MARGEN_PASO_HZ, F_INT + MARGEN_PASO_HZ]


def _disenar():
    return ft.disenar_iir(FS, WP, WS, GPASS_DB, GSTOP_DB, 'ellip')


def test_la_banda_de_paso_no_invade_los_tonos_utiles():
    for f in UTILES:
        assert abs(f - F_INT) > MARGEN_PASO_HZ, \
            f'{f} Hz cae dentro de la transicion del filtro ({WP} Hz)'


def test_el_filtro_es_estable():
    sos, _, _ = _disenar()
    estable, radio = ft.es_estable(sos)
    assert estable, f'radio polar maximo = {radio}'


def test_cumple_las_especificaciones_de_paso_y_rechazo():
    sos, _, _ = _disenar()
    r = ft.respuestas(sos, FS, n=8192)
    f, mag = r['f'], r['mag_db']
    rechazo = (f >= WS[0]) & (f <= WS[1])
    paso = (f <= WP[0]) | (f >= WP[1])
    assert mag[rechazo].max() <= -GSTOP_DB + 0.1, f'{mag[rechazo].max():.2f} dB'
    assert mag[paso].min() >= -GPASS_DB - 0.05, f'{mag[paso].min():.3f} dB'


def test_la_implementacion_propia_coincide_con_scipy_en_los_tres_casos():
    sos, b, a = _disenar()
    for identificador in CASOS:
        x = senales.cargar_caso(identificador)['contaminada']
        v = ft.verificar_equivalencia(sos, b, a, x, tol=TOLERANCIA_EQUIVALENCIA)
        assert v['ok'], f"{identificador}: {v['err_max_sos']:.2e} / {v['err_max_ba']:.2e}"


def test_atenua_la_interferencia_y_conserva_los_tonos_utiles():
    sos, _, _ = _disenar()
    n = np.arange(int(FS))
    x = np.sin(2*np.pi*F_INT*n/FS)
    y = ft.filtrar_sos_manual(sos, x)
    atenuacion = 20*np.log10(np.std(y[N_TRANSITORIO:])/np.std(x[N_TRANSITORIO:]))
    assert atenuacion < -(GSTOP_DB - 5), f'{atenuacion:.1f} dB'
    for f in UTILES:
        x = np.sin(2*np.pi*f*n/FS)
        y = ft.filtrar_sos_manual(sos, x)
        cambio = 20*np.log10(np.std(y[N_TRANSITORIO:])/np.std(x[N_TRANSITORIO:]))
        assert abs(cambio) < GPASS_DB + 0.1, f'{f} Hz: {cambio:+.2f} dB'


def test_la_snr_mejora_en_el_caso_1():
    sos, _, _ = _disenar()
    datos = senales.cargar_caso('caso_1')
    y = ft.filtrar_sos_manual(sos, datos['contaminada'])
    antes = ft.snr_db(datos['limpia'][N_TRANSITORIO:], datos['contaminada'][N_TRANSITORIO:])
    despues = ft.snr_db(datos['limpia'][N_TRANSITORIO:], y[N_TRANSITORIO:])
    assert despues > antes + 3, f'{antes:.2f} -> {despues:.2f} dB'


def test_la_cuantizacion_a_14_bits_conserva_la_estabilidad():
    sos, _, _ = _disenar()
    assert ft.es_estable(ft.cuantizar_sos(sos, 14))[0]


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
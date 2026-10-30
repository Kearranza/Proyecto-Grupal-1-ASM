import copy
import unittest

import numpy as np
from scipy import signal

import sistema_completo as sistema


class SistemaTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.p = sistema.cargar()
        cls.resultados = [sistema.correr_modo(cls.p, m['id'])
                          for m in cls.p['emisor']['modos']]

    def test_diagnostico_y_mejora(self):
        for r in self.resultados:
            interferencia = r['modo'] == 3
            self.assertEqual(r['diagnostico']['tipo'],
                             'tonal' if interferencia else 'sin_interferencia')
            self.assertEqual(r['cadena']['recortes_pwm'], 0)
            self.assertEqual(r['cadena']['recortes_adc'], 0)
            for fila in r['metricas']:
                self.assertGreater(fila['energia_util'], self.p['b']['energia_util_minima_prueba'])
                self.assertLess(fila['energia_util'], self.p['b']['energia_util_maxima_prueba'])
                if interferencia:
                    self.assertGreater(fila['delta_snr_db'],
                                       self.p['b']['snr_mejora_minima_prueba_db'])
                else:
                    self.assertIsNone(fila['atenuacion_db'])
            for x in r['senales'].values():
                self.assertEqual(len(x), self.p['n_muestras'])
                self.assertTrue(np.all(np.isfinite(x)))

    def test_reconstruccion_extremos_y_longitudes(self):
        n = self.p['n_bloque']
        generador = np.random.default_rng(12)
        for longitud in (1, n-1, n, 2*n+37):
            x = generador.normal(size=longitud)
            d = {'tipo': 'sin_interferencia', 'dc': 0.0}
            y = sistema.nodo_3_espectral(x, d, self.p)
            np.testing.assert_allclose(x, y, atol=self.p['b']['tolerancia_reconstruccion'])

    def test_temporal_contra_scipy(self):
        r = self.resultados[2]
        sos = sistema.disenar_temporal(r['diagnostico'], self.p)
        x = r['senales']['adquirida'] - r['diagnostico']['dc']
        np.testing.assert_allclose(signal.sosfilt(sos, x), r['senales']['temporal'], atol=1e-12)

    def test_cuantizacion_y_saturacion(self):
        cadena = sistema.cadena_fisica(np.array([-2., -1., 0., 1., 2.]), self.p)
        self.assertEqual(cadena['recortes_pwm'], 2)
        self.assertEqual(cadena['senal'][0], -1)
        self.assertEqual(cadena['senal'][-1], 1)
        np.testing.assert_array_equal(cadena['adc'], np.rint(cadena['adc']))

    def test_rc_opcional(self):
        p = copy.deepcopy(self.p)
        p['emisor']['filtro_reconstruccion']['fc_hz'] = p['fs_hz']/8
        cadena = sistema.cadena_fisica(np.zeros(p['n_bloque']), p)
        self.assertTrue(cadena['rc_activo'])
        self.assertTrue(np.all(np.isfinite(cadena['senal'])))
        self.assertEqual(cadena['recortes_adc'], 0)

    def test_escala_comun_y_snr(self):
        limpia, contaminada = sistema.entrada(self.p, 3)
        snr = sistema.metricas.snr_db(limpia, contaminada)
        self.assertAlmostEqual(snr, self.p['casos']['caso_1']['snr_objetivo_db'], places=2)
        self.assertAlmostEqual(np.max(np.abs(contaminada)), self.p['emisor']['nivel_pico'])


if __name__ == '__main__':
    unittest.main()

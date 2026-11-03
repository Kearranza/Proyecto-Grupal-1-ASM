"""Escribe config_generado.h a partir de config/sistema.json.

El firmware no puede leer JSON, asi que los numeros compartidos se pasan a C con
este script. Volver a correrlo cada vez que cambie sistema.json y commitear los
dos archivos juntos.

    python proyecto/pc/generar_config_h.py
"""
import json
from pathlib import Path

PROYECTO = Path(__file__).resolve().parent.parent
ENTRADA = PROYECTO / 'config' / 'sistema.json'
SALIDA = PROYECTO / 'firmware' / 'libraries' / 'asm_comun' / 'src' / 'config_generado.h'


def cargar():
    return json.loads(ENTRADA.read_text(encoding='utf-8'))


def main():
    c = cargar()
    b, e, p = c['bloque'], c['enlace'], c['pines']
    for n in b['n_permitidos']:
        if n & (n - 1) or n > b['n_max']:
            raise ValueError(f'{n} no es potencia de dos o pasa de n_max')

    definiciones = {
        'FS_HZ': c['muestreo']['fs_hz'],
        'N_POR_DEFECTO': b['n_por_defecto'],
        'N_MAX': b['n_max'],
        'BAUDIOS': e['baudios'],
        'TRAMA_SYNC0': hex(e['sync'][0]),
        'TRAMA_SYNC1': hex(e['sync'][1]),
        'TRAMA_TIPO_MUESTRAS': e['tipos']['muestras'],
        'TRAMA_TIPO_DIAGNOSTICO': e['tipos']['diagnostico'],
        'TRAMA_TIPO_COMANDO': e['tipos']['comando'],
        'TRAMA_TIPO_ESTADO': e['tipos']['estado'],
        'TRAMA_CARGA_MAX': e['carga_max_bytes'],
        'PICO_PIN_ADC': p['pico_w']['adc'],
        'PICO_PIN_UART_TX': p['pico_w']['uart_tx'],
        'PICO_PIN_UART_RX': p['pico_w']['uart_rx'],
        'ESP_PIN_UART_RX': p['esp32']['uart_rx'],
        'ESP_PIN_UART_TX': p['esp32']['uart_tx'],
        'ESP_PIN_DAC_TEMPORAL': p['esp32']['dac_temporal'],
        'ESP_PIN_DAC_ESPECTRAL': p['esp32']['dac_espectral'],
        'ADC_BITS': c['adc']['bits'],
        'DAC_BITS': c['dac']['bits'],
    }
    lineas = ['// Generado por pc/generar_config_h.py desde config/sistema.json. No editar a mano.',
              '#pragma once', '']
    lineas += [f'#define {k} {v}' for k, v in definiciones.items()]
    SALIDA.write_text('\n'.join(lineas) + '\n', encoding='utf-8')
    print(f'Escrito {SALIDA.relative_to(PROYECTO)} ({len(definiciones)} constantes)')


if __name__ == '__main__':
    main()

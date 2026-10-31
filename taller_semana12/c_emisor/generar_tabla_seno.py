"""BLOQUE C - Genera tabla_seno.h para el emisor de la Pico.

La Pico no calcula sin() en cada muestra: usa una tabla de un periodo y un
acumulador de fase de 32 bits (DDS). Este script escribe esa tabla en Q15
(enteros de 16 bits que representan [-1, 1)) para que el sketch la incluya.

    python taller_semana12/c_emisor/generar_tabla_seno.py

Volver a correrlo solo si cambia TAMANO_TABLA (debe ser potencia de dos para
indexar con los bits altos de la fase).
"""
from pathlib import Path

import numpy as np

TAMANO_TABLA = 256
SALIDA = Path(__file__).resolve().parent / 'emisor_pico' / 'tabla_seno.h'


def main():
    bits_indice = int(np.log2(TAMANO_TABLA))
    if 2**bits_indice != TAMANO_TABLA:
        raise ValueError('TAMANO_TABLA debe ser potencia de dos')

    n = np.arange(TAMANO_TABLA)
    q15 = np.round(32767 * np.sin(2 * np.pi * n / TAMANO_TABLA)).astype(int)

    filas = [', '.join(f'{v:6d}' for v in q15[i:i+8]) for i in range(0, TAMANO_TABLA, 8)]
    texto = (
        '// Generado por generar_tabla_seno.py. No editar a mano.\n'
        '#pragma once\n#include <stdint.h>\n\n'
        f'#define TABLA_BITS {bits_indice}\n'
        f'#define TABLA_N {TAMANO_TABLA}\n\n'
        'static const int16_t TABLA_SENO[TABLA_N] = {\n  '
        + ',\n  '.join(filas) + '\n};\n'
    )
    SALIDA.write_text(texto, encoding='utf-8')
    print(f'Escrito {SALIDA} ({TAMANO_TABLA} muestras, Q15)')


if __name__ == '__main__':
    main()

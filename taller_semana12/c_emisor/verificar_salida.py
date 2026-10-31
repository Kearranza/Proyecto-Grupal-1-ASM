"""BLOQUE C - Verifica que la salida de la Pico tiene las frecuencias pedidas.

Evidencia del punto 2 del taller. La captura puede venir de:
  - el osciloscopio (CSV con columnas tiempo, voltaje), o
  - una grabacion de la salida filtrada (WAV), o
  - las lecturas del ADC de la propia Pico (CSV de una columna, ver
    verificar_emisor.py; se asume fs = FS_DEFECTO_HZ).
Se calcula su espectro y se compara contra los modos de parametros.json.

    python taller_semana12/c_emisor/verificar_salida.py captura.csv --modo 2

Las amplitudes se comparan RELATIVAS a la primera componente del modo. Como la
captura se toma despues del filtro RC, el error en dB incluye su respuesta
(unos -0,3 dB en 3500 Hz con R = 470 ohm y C = 10 nF).

Entrega de C en el PDF (informe/secciones/3_emisor.tex):
  1. Por que DDS + tabla + PWM, como se obtiene fs exacta, tamano de tabla, Q15 y error de frecuencia esperado.
  2. Diagrama de conexion Pico (GPIO15) -> filtro RC -> salida, con valores de R y C.
  3. Filtro de reconstruccion: fc, por que esa fc, atenuacion de la portadora PWM.
  4. Figura: espectro medido vs esperado para cada modo, con el error en Hz y dB.
"""
import argparse
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comun import parametros as P  # noqa: E402
from comun.pasos import ejecutar  # noqa: E402

P.usar_src()

FS_DEFECTO_HZ = 16000.0     # para capturas de una sola columna (sin tiempo)
SEPARACION_MIN_BINS = 5     # dos picos mas cerca que esto son el mismo tono


def leer_captura(ruta):
    """Devuelve (x, fs) desde CSV del osciloscopio o WAV."""
    if ruta is None:
        sys.exit('Indica la captura: verificar_salida.py captura.csv --modo N')
    ruta = Path(ruta)
    if not ruta.exists():
        sys.exit(f'No existe {ruta}')

    if ruta.suffix.lower() == '.wav':
        from scipy.io import wavfile
        fs, datos = wavfile.read(ruta)
        x = datos.astype(float)
        if x.ndim > 1:
            x = x[:, 0]                       # primer canal
        return x - x.mean(), float(fs)

    filas = []                                # CSV: se saltan encabezados no numericos
    for linea in ruta.read_text(encoding='utf-8', errors='ignore').splitlines():
        try:
            filas.append([float(v) for v in linea.replace(';', ',').split(',') if v.strip()])
        except ValueError:
            continue
    if not filas:
        sys.exit(f'{ruta} no tiene datos numericos')
    ancho = min(len(f) for f in filas)
    datos = np.array([f[:ancho] for f in filas])

    if ancho == 1:
        x, fs = datos[:, 0], FS_DEFECTO_HZ
    else:                                     # columnas: tiempo, voltaje
        t, x = datos[:, 0], datos[:, 1]
        fs = 1.0 / float(np.median(np.diff(t)))
    return x - x.mean(), float(fs)


def picos(x, fs, cuantos):
    """Frecuencia y nivel (dB) de los 'cuantos' picos mayores, con ventana Hann.

    Devuelve [(f_hz, nivel_db), ...] ordenado por frecuencia. El nivel es la
    amplitud del tono (en las unidades de la captura) en dB, estimada sumando el
    lobulo principal (+-2 bins) para no depender de que el tono caiga en un bin.
    """
    N = len(x)
    w = 0.5 - 0.5 * np.cos(2 * np.pi * np.arange(N) / N)
    X = np.fft.rfft(x * w)
    pot = np.abs(X) ** 2
    sw2 = float(np.sum(w ** 2))
    df = fs / N

    locales = [k for k in range(2, len(pot) - 2)
               if pot[k] > pot[k - 1] and pot[k] >= pot[k + 1]]
    locales.sort(key=lambda k: pot[k], reverse=True)

    elegidos = []
    for k in locales:
        if all(abs(k - j) >= SEPARACION_MIN_BINS for j in elegidos):
            elegidos.append(k)
        if len(elegidos) == cuantos:
            break

    resultado = []
    for k in elegidos:
        l0, l1, l2 = np.log(pot[k - 1:k + 2] + 1e-30)
        delta = 0.5 * (l0 - l2) / (l0 - 2 * l1 + l2)            # interpolacion parabolica
        amplitud = 2 * np.sqrt(pot[k - 2:k + 3].sum() / (N * sw2))
        resultado.append(((k + delta) * df, 20 * np.log10(amplitud)))
    return sorted(resultado)


def comparar(medidos, m):
    """Error en Hz y dB de cada componente medida contra el modo m del JSON."""
    esperadas = list(zip(m['frecuencias_hz'], m['amplitudes']))
    if len(medidos) < len(esperadas):
        print(f'  [FALLA] se encontraron {len(medidos)} picos y se esperaban {len(esperadas)}')

    def cercano(f):
        return min(medidos, key=lambda p: abs(p[0] - f)) if medidos else None

    ref_f, ref_a = esperadas[0]
    ref_nivel = cercano(ref_f)[1]
    filas = []
    print(f'  {"f esp":>7s} {"f med":>9s} {"err (Hz)":>9s} {"rel esp":>9s} {"rel med":>9s} {"err (dB)":>9s}')
    for f, a in esperadas:
        pico = cercano(f)
        if pico is None:
            continue
        rel_esp = 20 * np.log10(a / ref_a)
        rel_med = pico[1] - ref_nivel
        fila = dict(f_esp=f, f_med=pico[0], err_hz=pico[0] - f,
                    rel_esp_db=rel_esp, rel_med_db=rel_med, err_db=rel_med - rel_esp)
        filas.append(fila)
        print(f'  {f:7.0f} {pico[0]:9.1f} {fila["err_hz"]:+9.2f} {rel_esp:+8.2f}  {rel_med:+8.2f}  '
              f'{fila["err_db"]:+8.2f}')
    return filas


def main():
    p = P.cargar()
    ap = argparse.ArgumentParser()
    ap.add_argument('captura', nargs='?')
    ap.add_argument('--modo', type=int, default=2)
    args = ap.parse_args()
    m = P.modo(p, args.modo)

    estado = {}
    ejecutar(f'Bloque C - verificacion del modo {m["id"]}', [
        ('leer captura', lambda: estado.update(zip(('x', 'fs'), leer_captura(args.captura)))),
        ('picos', lambda: estado.update(picos=picos(estado['x'], estado['fs'],
                                                    len(m['frecuencias_hz'])))),
        ('comparar', lambda: comparar(estado['picos'], m)),
    ], encadenado=True)


if __name__ == '__main__':
    main()
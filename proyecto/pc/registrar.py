"""Registra por serial lo que mandan las placas, como evidencia de las pruebas (Kevin).

    pip install pyserial
    python proyecto/pc/registrar.py --puerto COM7 --segundos 10 --salida proyecto/resultados/P1_r1

Guarda las tramas decodificadas (muestras, diagnostico, estado) y los contadores
del lector (errores de CRC, bloques perdidos) para el paper.

Sin placa:
    --archivo captura.bin   relee un volcado crudo de la UART
    --simular               arma un flujo de prueba y lo decodifica
"""
import argparse
import json
import sys
import re
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import trama as T  # noqa: E402

ESTADO = ('desbordamientos', 'saltos_procesados', 'latencia_ms')


def _vacio():
    return {'muestras': [], 'seq': [], 'diagnostico': [], 'estado': []}


def procesar(recolectado, tramas):
    # Reparte las tramas por tipo. El orden de llegada se conserva en 'seq'
    for tipo, seq, carga in tramas:
        if tipo == T.TIPOS['muestras']:
            recolectado['muestras'].append(np.frombuffer(carga, dtype='<i2'))
            recolectado['seq'].append(seq)
        elif tipo == T.TIPOS['diagnostico'] and len(carga) >= T.DIAGNOSTICO.size:
            clase, _, bloque, frecuencia, ancho, nivel = T.DIAGNOSTICO.unpack_from(carga)
            recolectado['diagnostico'].append(
                {'seq': seq, 'tipo': clase, 'n_bloque': bloque,
                 'frecuencia_hz': frecuencia, 'ancho_hz': ancho, 'nivel_db': nivel})
        elif tipo == T.TIPOS['estado']:
            valores = np.frombuffer(carga[:4*len(ESTADO)], dtype='<u4')
            recolectado['estado'].append({'seq': seq, **dict(zip(ESTADO, valores.tolist()))})


FONDO_ESCALA = 1 << (T.SISTEMA['adc']['bits'] - 1)   # el nodo 1 manda cuentas centradas


def texto_suelto(crudo):
    """Lo que viaja entre tramas y no es trama: las respuestas de los comandos.

    Con el enlace activo la consola de la sonda es ilegible, porque las tramas son
    binario. Aqui se rescatan las lineas de texto que quedaron entre medio, que es
    donde salen las respuestas a TOCAR, PARAR y ESTADO.
    """
    lineas = []
    for trozo in re.findall(rb'[\x20-\x7e]{10,}', crudo):
        texto = trozo.decode('ascii', 'ignore')
        # la respuesta puede venir pegada a bytes de una trama vecina que son imprimibles
        for marca in ('OK ', 'ERROR ', 'prueba='):
            if marca in texto:
                linea = texto[texto.index(marca):].strip()
                if linea not in lineas:
                    lineas.append(linea)
                break
    return lineas


def diagnostico_nivel(recolectado):
    """Aviso de nivel: saturar o quedarse corto arruina la captura y no se ve en los contadores."""
    import numpy as np
    x = np.concatenate(recolectado['muestras']) if recolectado['muestras'] else np.array([])
    if x.size == 0:
        print('  nivel: no llegaron muestras')
        return
    pico = int(np.max(np.abs(x)))
    uso = 100.0*pico/FONDO_ESCALA
    recortadas = int(np.sum(np.abs(x) >= FONDO_ESCALA - 8))
    continua = float(np.mean(x))
    print(f'  nivel: pico {uso:.0f} % de la escala, continua {continua:+.0f} '
          f'de {FONDO_ESCALA}, {recortadas} muestras al tope')
    if recortadas > 0:
        print('    ATENCION: esta recortando. Bajar el volumen o alejar el microfono')
    elif uso < 10:
        print('    ATENCION: muy bajo, el ruido de cuantizacion se va a comer la medida')
    elif uso > 90:
        print('    al limite: conviene bajar un poco')
    else:
        print('    nivel bien')


def guardar(recolectado, lector, salida, metadatos):
    salida = Path(salida)
    salida.parent.mkdir(parents=True, exist_ok=True)
    muestras = (np.concatenate(recolectado['muestras']) if recolectado['muestras']
                else np.zeros(0, dtype='<i2'))
    np.savez_compressed(salida.with_suffix('.npz'), muestras=muestras,
                        seq=np.array(recolectado['seq'], dtype='<u2'))
    resumen = {
        **metadatos,
        'muestras': int(muestras.size),
        'tramas_ok': lector.tramas_ok,
        'errores_crc': lector.errores_crc,
        'bloques_perdidos': lector.bloques_perdidos,
        'bytes_descartados': lector.bytes_descartados,
        'diagnostico': recolectado['diagnostico'],
        'estado': recolectado['estado'],
    }
    salida.with_suffix('.json').write_text(
        json.dumps(resumen, ensure_ascii=False, indent=2), encoding='utf-8')
    return resumen


def flujo_simulado(segundos, fs=16000, n=1024):
    # Flujo de prueba: tono de 440 Hz troceado en saltos de N/2, para verificar
    # el camino completo de decodificacion sin depender de una placa
    salto = n // 2
    total = int(fs*segundos)
    t = np.arange(total)/fs
    senal = np.int16(np.round(12000*np.sin(2*np.pi*440*t)))
    datos = bytearray()
    for seq, inicio in enumerate(range(0, total - salto + 1, salto)):
        datos += T.armar(T.TIPOS['muestras'], seq, senal[inicio:inicio+salto].tobytes())
    return bytes(datos)


def registrar(puerto, segundos, salida, archivo=None, simular=False):
    lector, recolectado, crudo_todo = T.Lector(), _vacio(), bytearray()
    metadatos = {'puerto': puerto, 'segundos': segundos,
                 'fecha': time.strftime('%Y-%m-%d %H:%M:%S')}

    if simular or archivo:
        crudo = flujo_simulado(segundos) if simular else Path(archivo).read_bytes()
        metadatos['origen'] = 'simulado' if simular else str(archivo)
        crudo_todo += crudo
        procesar(recolectado, lector.alimentar(crudo))
    else:
        try:
            import serial
        except ImportError:
            raise SystemExit('falta pyserial: pip install pyserial')
        metadatos['origen'] = puerto
        metadatos['baudios'] = T.SISTEMA['enlace']['baudios']
        with serial.Serial(puerto, T.SISTEMA['enlace']['baudios'], timeout=0.1) as puerto_serie:
            fin = time.time() + segundos
            while time.time() < fin:
                datos = puerto_serie.read(4096)
                if datos:
                    crudo_todo += datos
                    procesar(recolectado, lector.alimentar(datos))

    resumen = guardar(recolectado, lector, salida, metadatos)
    print(f"  {resumen['muestras']} muestras | {resumen['tramas_ok']} tramas | "
          f"{resumen['errores_crc']} errores de CRC | "
          f"{resumen['bloques_perdidos']} bloques perdidos")
    diagnostico_nivel(recolectado)
    for linea in texto_suelto(bytes(crudo_todo)):
        print(f'  dijo la placa: {linea}')
    return resumen


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--puerto', default='-')
    ap.add_argument('--segundos', type=float, default=10.0)
    ap.add_argument('--salida', required=True)
    ap.add_argument('--archivo', help='volcado crudo de la UART, en vez del puerto')
    ap.add_argument('--simular', action='store_true', help='flujo de prueba, sin placa')
    a = ap.parse_args()
    registrar(a.puerto, a.segundos, a.salida, a.archivo, a.simular)


if __name__ == '__main__':
    main()

"""Corre una lista de pasos y reporta [ok] / [PENDIENTE] / [ERROR], como en el taller."""
import traceback


def ejecutar(titulo, pasos):
    print(f'\n== {titulo} ==')
    listos = 0
    for nombre, funcion in pasos:
        try:
            funcion()
            listos += 1
            print(f'  [ok]        {nombre}')
        except NotImplementedError as e:
            print(f'  [PENDIENTE] {nombre}: {e}')
        except Exception:
            print(f'  [ERROR]     {nombre}')
            traceback.print_exc()
    print(f'  -> {listos}/{len(pasos)} pasos listos')

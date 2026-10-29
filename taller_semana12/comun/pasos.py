"""Corre una lista de pasos y reporta cuales faltan.

Cada bloque arma su main() como una lista de pasos. Un paso que todavia no esta
hecho lanza NotImplementedError y aqui sale como PENDIENTE, sin tumbar los demas.
Asi cualquiera puede correr cualquier script y ver en que va cada parte.

Con encadenado=True cada paso usa el resultado del anterior, asi que despues del
primer paso que no se completa los siguientes salen como "espera".
"""
import traceback


def ejecutar(titulo, pasos, encadenado=False):
    print(f'\n== {titulo} ==')
    resultados, listos, bloqueado = {}, 0, False
    for nombre, funcion in pasos:
        if bloqueado:
            print(f'  [espera]    {nombre}')
            continue
        try:
            resultados[nombre] = funcion()
            listos += 1
            print(f'  [ok]        {nombre}')
        except NotImplementedError as e:
            print(f'  [PENDIENTE] {nombre}: {e}')
            bloqueado = encadenado
        except Exception:
            print(f'  [ERROR]     {nombre}')
            traceback.print_exc()
            bloqueado = encadenado
    print(f'  -> {listos}/{len(pasos)} pasos listos')
    return resultados

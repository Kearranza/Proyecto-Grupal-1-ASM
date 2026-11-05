# Métricas del Nodo 3

El módulo acumula sumas sin guardar toda la señal. Mantener dos instancias de
`metricas_t`: una para la salida temporal y otra para la espectral. Reiniciarlas
entre repeticiones, condiciones, cambios de N o pérdidas de muestras. Las
funciones devuelven ASM_OK o ASM_ERROR.

## MSE y SNR

Pasar a `metricas_acumular` referencia limpia s, entrada contaminada x y salida y
con igual escala, igual longitud y **alineadas en el tiempo**.

- MSE = sum((s-y)²) / cantidad de muestras.
- SNR de entrada = 10 log10(sum(s²) / sum((s-x)²)).
- SNR de salida = 10 log10(sum(s²) / sum((s-y)²)).
- Mejora = 10 log10(sum((s-x)²) / sum((s-y)²)).

Las sumas y restas se realizan en double; el resultado público es float. La
referencia limpia no nula y un error exactamente cero producen SNR +infinita.
Una referencia silenciosa con error positivo produce SNR -infinita. El cociente
0/0 y la mejora con referencia silenciosa producen NaN.

Para el filtro espectral, la muestra de salida i corresponde a la muestra de
entrada i-N/2. Mantener un historial de la referencia y la entrada contaminada
para pasar ese segmento a las métricas. Excluir el relleno inicial; al terminar,
vaciar el filtro y evaluar solo las muestras correspondientes a la longitud
original.

## Energía útil y atenuación

`metricas_energia_banda` toma los arreglos de una FFT completa de una señal real,
sin escala en la transformada directa. Calcula energía por Parseval: suma
|X[k]|²/N de la banda, duplicando los bins positivos interiores para contar sus
conjugados. DC y Nyquist se cuentan una sola vez. Los límites en Hz son
inclusivos; una banda sin centros de bins tiene energía cero, sin interpolación.

Definir las bandas de evaluación **antes** de medir. Calcular la energía de:

1. Referencia limpia en las bandas útiles.
2. Salida filtrada en esas mismas bandas.
3. Entrada contaminada en la banda de interferencia principal.
4. Salida filtrada en esa misma banda de interferencia.

El porcentaje reportado es 100 * energía útil de salida / energía útil de
referencia. La atenuación es 10 log10(energía de interferencia de entrada /
energía de interferencia de salida). Se calculan a partir de las sumas acumuladas,
no promediando porcentajes o decibelios por bloque.

La energía en una banda útil puede incluir ruido residual: este porcentaje es
una medida **por bandas**, no una separación perfecta entre fuentes. Puede
superar 100 % por ganancia o ruido residual y no se recorta. Si señal y ruido se
superponen, explicar esa limitación y analizar también MSE y SNR. La atenuación
por banda tampoco distingue interferencia de señal útil superpuesta.

## Tiempo y carga

Medir externamente el procesamiento de cada salto, por ejemplo con la resta
`uint32_t transcurrido = micros() - inicio;`, y pasarlo a
`metricas_acumular_tiempo` junto con el período disponible. La resta modular
funciona si el intervalo medido es menor que una vuelta del contador.
Para un salto de N/2, el período es 1e6*N/(2*fs) microsegundos.

Se reportan tiempo medio, tiempo máximo y carga aproximada:
100 * suma de tiempos de cómputo / suma de períodos disponibles. Una carga
superior a 100 % indica que no se cumple el plazo promedio. El máximo permite
identificar saltos lentos incluso con promedio aceptable. Esto **no** mide toda
la latencia ADC–DAC ni el uso global de CPU: faltan transporte, colas y otras
tareas. La memoria estática puede reportarse con `sizeof(metricas_t)` y
`sizeof(espectral_t)` en la placa.

## Resultados

`metricas_calcular` requiere al menos una muestra temporal. Si no se acumularon
bandas o tiempos, sus resultados son NaN; la interfaz debe mostrarlos como
«sin medición», no como cero. Una energía útil de referencia nula también deja
su porcentaje indefinido. Recompilar todos los consumidores: se añadieron campos
a las estructuras, aunque las tres funciones originales conservan sus firmas.
Estas estructuras son estado local, no un nuevo formato de trama.

## Pruebas

Desde la raíz: `make -C proyecto/pruebas_c test_metricas`.
Se verifica la referencia existente, fórmulas con valores conocidos, partición
en bloques, silencio, error cero, datos inválidos, bandas, Parseval, tiempos y
el desbordamiento del contador de muestras.

# Filtro espectral y reconstrucción

`espectral.c` procesa N/2 muestras por llamada, con N potencia de dos entre 8 y
N_MAX. El sistema usa 256, 512, 1024 o 2048. No reserva memoria dinámica y cada
instancia guarda su propio historial: no compartir una instancia entre tareas.
El estado existente ocupa aproximadamente 32 KiB con N_MAX = 2048.

## Flujo y reconstrucción

1. Conservar la mitad anterior y agregar las muestras nuevas, ya centradas y
   normalizadas por el receptor. No se elimina la media de cada bloque.
2. Aplicar Hann **periódica**, w[i] = (1 - cos(2*pi*i/N))/2, y calcular FFT.
3. Aplicar ganancias reales a los bins positivos y escribir sus conjugados.
   DC y Nyquist se mantienen reales.
4. Calcular IFFT (incluye 1/N) y sumar la primera mitad con la cola anterior.
   Guardar la segunda mitad para la siguiente llamada.

No se aplica una segunda ventana. Con salto N/2, las ventanas periódicas
adyacentes suman uno. Sin rechazo, la salida reproduce la entrada con N/2
muestras de retraso, también al inicio gracias al historial inicial en cero.
Este retraso es de 8, 16, 32 o 64 ms a 16 kHz para los tamaños del sistema.
La latencia física también depende de adquisición, cómputo, colas y DAC.

Para una señal finita, completar el último salto con ceros si es necesario.
Enviar después un salto de ceros para recuperar las últimas muestras alineadas
con la entrada. Un segundo salto vacía la cola temporal del filtro. Recortar
N/2 muestras iniciales y conservar la longitud original al comparar métricas.


## Verificación en computadora

Ejecutar `make -C proyecto/pruebas_c test_espectral` desde la raíz.
Las pruebas cubren identidad con DC, Nyquist y señal no periódica; inicio y
vaciado; tonos dentro y fuera de bins; cambio de frecuencia; rechazo de banda;
SNR con ruido blanco sintético; ajuste de parámetros; procesamiento en sitio;
argumentos inválidos y reinicio.

# 2.4 Reducción espectral mediante FFT e IFFT

La reducción de ruido espectral se implementó utilizando procesamiento por bloques con FFT e IFFT. Cada bloque fue multiplicado por una ventana Hann, transformado al dominio de la frecuencia, procesado mediante una estrategia adaptativa y finalmente reconstruido utilizando IFFT y overlap-add.

## a) Bloques de tamaño potencia de dos

Se utilizó un tamaño de bloque de 1024 muestras.

Esta longitud corresponde a una potencia de dos y permite utilizar directamente la FFT radix-2 desarrollada en el proyecto.

Para una frecuencia de muestreo de 16 kHz la resolución espectral es

Δf = fs / N = 16000 / 1024 = 15.625 Hz

lo cual permite separar adecuadamente los tonos útiles de 440 Hz, 880 Hz y 1320 Hz.

---

## b) Preservación de la simetría conjugada

Como la señal temporal es real, la FFT cumple

X[k] = X*[N-k]

Después de modificar cada bloque espectral se reconstruyó explícitamente esta relación antes de ejecutar la IFFT.

De esta forma la señal resultante permanece real y se evitan componentes imaginarias debidas al procesamiento numérico.

---

## c) Parámetro adaptativo

El método estima automáticamente las características del ruido de cada bloque.

Se utilizaron tres métricas:

- planitud espectral;
- relación entre pico máximo y mediana espectral;
- distribución de energía en frecuencia.

A partir de estas medidas se clasifica el bloque como:

- ruido tonal;
- ruido blanco;
- ruido de banda.

La estrategia de filtrado se selecciona automáticamente para cada caso.

---

## d) Solapamiento

Se utilizó una ventana Hann con 50 % de solapamiento.

Para bloques de 1024 muestras se empleó un desplazamiento de 512 muestras entre bloques consecutivos.

La reconstrucción se realizó mediante overlap-add para reducir discontinuidades y artefactos en los bordes de bloque.

---

## e) Aplicación a los tres casos

### Caso 1

La interferencia tonal de 3500 Hz aparece como un pico dominante claramente separado de la banda útil.

El algoritmo detecta automáticamente este comportamiento y aplica una atenuación estrecha centrada en la frecuencia interferente.

### Caso 2

El ruido blanco distribuye su energía a lo largo de todo el espectro.

Se utiliza una atenuación proporcional al nivel de ruido estimado mediante la mediana espectral.

### Caso 3

El ruido ocupa la banda comprendida entre 800 Hz y 1500 Hz.

El algoritmo identifica automáticamente una región extensa de energía elevada y aplica una reducción selectiva sobre esa banda.

---

## f) Compromiso entre reducción de ruido y distorsión

El caso 3 demuestra la principal limitación del filtrado espectral.

Las componentes útiles de 880 Hz y 1320 Hz pertenecen a la misma banda donde se encuentra el ruido.

Por esta razón una atenuación más agresiva reduce simultáneamente la energía del ruido y la energía útil.

Este resultado confirma que cuando señal y ruido comparten frecuencias no es posible eliminar completamente el ruido sin introducir distorsión en la señal reconstruida.
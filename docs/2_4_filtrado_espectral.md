# 2.4 Reducción espectral mediante FFT e IFFT

La reducción de ruido espectral se implementó mediante procesamiento por bloques utilizando la FFT radix-2 desarrollada en el proyecto y reconstrucción mediante IFFT. El algoritmo opera sobre bloques de longitud potencia de dos, aplica una ventana Hann, estima las características del ruido en cada bloque, modifica selectivamente el espectro y posteriormente reconstruye la señal mediante overlap-add.

Las figuras `fig_24_caso_1.png`, `fig_24_caso_2.png` y `fig_24_caso_3.png` muestran los resultados obtenidos para los tres escenarios de prueba.

## a) Bloques de tamaño potencia de dos

Se utilizó una longitud de bloque de 1024 muestras, coincidente con la configuración común definida para todo el proyecto.

Esta elección permite utilizar directamente la FFT radix-2 implementada en `src/dft_fft.py`, reduciendo significativamente el costo computacional respecto a una DFT calculada directamente.

Con una frecuencia de muestreo de 16 kHz se obtiene una resolución frecuencial

Δf = fs / N = 16000 / 1024 = 15.625 Hz

suficiente para distinguir claramente las componentes útiles de 440 Hz, 880 Hz y 1320 Hz, así como la interferencia tonal de 3500 Hz presente en el caso 1.

Adicionalmente, el tamaño de bloque coincide exactamente con el parámetro definido en `configuraciones.json`, lo cual facilita la futura migración del algoritmo a la plataforma embebida prevista para el proyecto.

---

## b) Preservación de la simetría conjugada

Las señales procesadas en este proyecto son reales. Debido a ello, la DFT cumple la propiedad de simetría conjugada

X[k] = X*[N-k]

donde (*) representa conjugación compleja.

Durante la reducción de ruido se modifican determinadas componentes espectrales asociadas al ruido detectado. Si estas modificaciones se aplicaran únicamente sobre una mitad del espectro, la simetría conjugada dejaría de cumplirse y la IFFT produciría componentes imaginarias artificiales.

Por esta razón, después de cada modificación espectral se reconstruyó explícitamente la relación de conjugación entre frecuencias positivas y negativas antes de ejecutar la IFFT.

La preservación de esta propiedad garantiza que la señal reconstruida permanezca real y evita errores numéricos durante la etapa de síntesis.

---

## c) Parámetro adaptativo

El método incorpora adaptación automática a partir del contenido espectral de cada bloque.

Para cada FFT se calcularon métricas simples que permiten caracterizar la distribución de energía del espectro:

- planitud espectral;
- relación entre el pico máximo y la mediana espectral;
- distribución general de energía.

A partir de estas medidas el algoritmo clasifica automáticamente el ruido observado en una de tres categorías:

- interferencia tonal dominante;
- ruido blanco;
- ruido de banda.

Dependiendo de dicha clasificación se selecciona una estrategia distinta de modificación espectral.

En consecuencia, el procesamiento no depende de una frecuencia fija conocida de antemano y satisface el requisito de adaptación solicitado en el enunciado.

---

## d) Solapamiento

Cada bloque se multiplicó por una ventana Hann antes de calcular la FFT.

La utilización de ventanas reduce la fuga espectral, pero introduce una atenuación progresiva en los extremos de cada bloque. Para evitar discontinuidades entre bloques consecutivos se empleó reconstrucción overlap-add con un solapamiento del 50 %.

Para una longitud de bloque de 1024 muestras se obtiene un desplazamiento entre bloques de

Hop = 512 muestras

lo que permite reconstruir adecuadamente la señal procesada y minimizar artefactos de borde.

---

## e) Aplicación a los tres casos

### Caso 1: interferencia tonal

El caso 1 contiene una interferencia sinusoidal localizada en 3500 Hz y separada de las frecuencias útiles.

La Figura `fig_24_caso_1.png` muestra que el algoritmo identifica correctamente esta componente dominante y reduce significativamente su energía sin afectar de forma apreciable las componentes útiles de 440 Hz, 880 Hz y 1320 Hz.

Este escenario corresponde al caso ideal para el filtrado espectral porque señal y ruido ocupan regiones claramente diferenciadas del espectro.

### Caso 2: ruido blanco

En el caso 2 la energía del ruido se distribuye a lo largo de todo el espectro.

La Figura `fig_24_caso_2.png` muestra una reducción general del piso de ruido manteniendo las componentes principales de la señal útil.

Debido a la naturaleza ancha del ruido no existe una única frecuencia interferente que eliminar, por lo que la reducción debe realizarse de manera distribuida sobre un gran número de bins espectrales.

### Caso 3: ruido de banda

El caso 3 incorpora ruido concentrado entre 800 Hz y 1500 Hz.

La Figura `fig_24_caso_3.png` muestra que la mayor parte de la modificación espectral se concentra precisamente dentro de dicha región.

Sin embargo, dos de las componentes útiles de la señal, 880 Hz y 1320 Hz, se encuentran dentro de esa misma banda. Por esta razón no resulta posible eliminar completamente el ruido sin introducir simultáneamente cierta degradación de la señal útil.

Este escenario representa el caso más exigente de los tres estudiados.

---

## f) Compromiso entre reducción de ruido y distorsión

Los resultados obtenidos permiten observar una limitación fundamental del filtrado espectral.

Cuando el ruido se encuentra separado de la señal útil, como ocurre en el caso 1, es posible eliminar una gran fracción de la interferencia con una distorsión mínima.

Por el contrario, cuando señal y ruido comparten frecuencias, como ocurre en el caso 3, cualquier intento de aumentar la atenuación del ruido también reduce parte de la energía de la señal deseada.

La Figura `fig_24_caso_3.png` muestra que la banda de mayor atenuación coincide con la región donde se encuentran las componentes útiles de 880 Hz y 1320 Hz. Esto confirma experimentalmente que existe un compromiso inevitable entre reducción de ruido y preservación de información cuando ambas ocupan la misma región espectral.

El comportamiento observado no constituye una falla del algoritmo sino una consecuencia directa de las limitaciones físicas del problema: la FFT permite separar componentes por frecuencia, pero no puede distinguir entre señal útil y ruido cuando ambos aparecen superpuestos en los mismos bins espectrales.
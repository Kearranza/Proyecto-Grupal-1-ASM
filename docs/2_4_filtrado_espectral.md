# 2.4 Reducción espectral mediante FFT e IFFT

## a) Bloques de tamaño potencia de dos

Se utilizó una longitud de bloque de 1024 muestras. Esta elección permite utilizar directamente la FFT radix‑2 implementada en el proyecto, reduciendo significativamente el costo computacional respecto a una DFT convencional.

Con una frecuencia de muestreo de 16 kHz se obtiene una resolución espectral

Δf = fs / N = 16000 / 1024 = 15.625 Hz.

Además, 1024 corresponde exactamente al tamaño de bloque definido en la configuración general del proyecto.

---

## b) Preservación de la simetría conjugada

La señal procesada es real, por lo que su transformada discreta de Fourier cumple

X[k] = X*[N-k]

donde (*) representa conjugación compleja.

Después de modificar cualquier componente espectral se reconstruyó explícitamente la componente conjugada correspondiente. De esta manera la IFFT genera nuevamente una señal temporal real.

La conservación de esta propiedad evita componentes imaginarias artificiales y garantiza una reconstrucción físicamente válida.

---

## c) Parámetro adaptativo

El algoritmo estima automáticamente el tipo de ruido presente en cada bloque.

Se utilizaron tres métricas:

- planitud espectral;
- relación entre el máximo espectral y la mediana;
- continuidad de bins energéticos.

Dependiendo del resultado se selecciona una estrategia diferente para ruido tonal, ruido blanco o ruido de banda.

Este comportamiento satisface el requisito de adaptación solicitado en el enunciado.

---

## d) Solapamiento

Se empleó una ventana Hann y reconstrucción overlap-add con un solapamiento del 50 %.

Para bloques de 1024 muestras:

- tamaño del bloque = 1024;
- salto entre bloques = 512 muestras.

Esto reduce discontinuidades en los bordes de cada bloque y mejora la calidad de la señal reconstruida.

---

## e) Aplicación a los tres casos

### Caso 1

La interferencia tonal en 3500 Hz aparece como un pico muy dominante y aislado.

El algoritmo detecta automáticamente esta condición y aplica una atenuación estrecha alrededor de la frecuencia interferente.

### Caso 2

El ruido blanco presenta energía distribuida a lo largo de todo el espectro.

Se emplea una reducción basada en el nivel medio de ruido estimado para cada bloque.

### Caso 3

El ruido ocupa una banda entre 800 Hz y 1500 Hz.

La detección automática identifica una región espectral extensa con energía elevada y aplica una atenuación selectiva dentro de dicha banda.

---

## f) Compromiso entre reducción de ruido y distorsión

El caso 3 evidencia la principal limitación del método.

Las componentes útiles de 880 Hz y 1320 Hz se encuentran dentro de la banda contaminada por ruido.

Al incrementar la agresividad de la atenuación se reduce más energía de ruido, pero simultáneamente se elimina parte de la energía útil.

Por tanto, existe un compromiso inevitable entre reducción de ruido y preservación de la señal cuando ambas comparten las mismas frecuencias.
``
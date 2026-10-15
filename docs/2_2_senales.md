# 2.2 Generación y caracterización de señales

Todo lo de esta sección lo produce `src/senales.py` a partir de
`metadatos/configuraciones.json`, y lo verifica `pruebas/test_senales.py`.
Las figuras las genera `src/graficas.py`.

## Parámetros comunes

| Parámetro | Valor | Razón |
|---|---|---|
| Frecuencia de muestreo `fs` | 16 000 Hz | la banda útil llega a 1320 Hz y la interferencia más alta a 3500 Hz; sobra margen sobre Nyquist |
| Duración | 2,048 s | son exactamente 32 768 muestras: potencia de dos y múltiplo entero del bloque |
| Muestras `N` | 32 768 | |
| Bloque de procesamiento | 1024 muestras | Δf = 15,6 Hz y 64 ms de latencia por bloque |
| Solapamiento | 50 % | |
| Ventana de análisis | Hann | |
| Pico digital | 0,80 | deja 1,9 dB de resguardo antes de saturar |
| Desvanecido | 20 ms | evita el clic al empezar y terminar la reproducción |
| Formato | WAV PCM mono 16 bits | lo que lee el generador del profesor |

La duración no es arbitraria. A 16 kHz, 2,048 s son 32 768 muestras, que es
2^15. Así el procesamiento por bloques del punto 2.4 divide la señal en 32
bloques exactos sin tener que rellenar el último con ceros, y la FFT propia
—que es radix-2— puede analizar la señal completa de una sola vez si hace falta.

## a) Los tres casos

| | caso_1 | caso_2 | caso_3 |
|---|---|---|---|
| Semilla | 20261001 | 20261002 | 20261003 |
| Señal útil | multitono 440 / 880 / 1320 Hz | igual | igual |
| Amplitudes relativas | 1,00 / 0,55 / 0,30 | igual | igual |
| Fases | 0° / 35° / −50° | igual | igual |
| Tipo de ruido | tonal en 3500 Hz | blanco | banda limitada 800–1500 Hz |
| SNR objetivo | 2,0 dB | 0,0 dB | 3,0 dB |

La señal útil es la misma en los tres casos a propósito: así cualquier
diferencia que se mida después se le puede atribuir al ruido y no a la señal.

El ruido de banda del caso 3 no se genera con un filtro de biblioteca sino
enmascarando el espectro de ruido blanco con un paso banda de bordes de coseno
elevado (50 Hz de transición). Es equivalente para el propósito y deja la
generación sin más dependencias que numpy.

## b) SNR obtenida

La amplitud del ruido se escala con

```
factor = sqrt( Σ s²[n] / ( Σ r²[n] · 10^(SNR/10) ) )
```

de modo que la SNR sale exacta por construcción. Después las tres señales se
normalizan con **una sola ganancia común**, que por ser común no altera la
relación entre ellas.

| Caso | SNR objetivo | SNR en punto flotante | SNR leída del WAV | Diferencia |
|---|---|---|---|---|
| caso_1 | 2,0 dB | 2,0000 dB | 1,9999 dB | −0,0001 dB |
| caso_2 | 0,0 dB | 0,0000 dB | −0,0003 dB | −0,0003 dB |
| caso_3 | 3,0 dB | 3,0000 dB | 2,9998 dB | −0,0002 dB |

La única diferencia es la cuantización a 16 bits, que es exactamente el
redondeo que el enunciado admite. En punto flotante la coincidencia es exacta
hasta el último dígito que guarda un `double`.

## c) Caracterización

Figuras `fig_22_caso_1.png`, `fig_22_caso_2.png` y `fig_22_caso_3.png`: cada una
trae señal en el tiempo, espectro de magnitud, fase y distribución de energía
por bandas. La figura `fig_22_comparacion.png` pone los tres espectros juntos.

Energía de la señal contaminada repartida por bandas, en porcentaje:

| Banda (Hz) | 0–250 | 250–500 | 500–1000 | 1000–2000 | 2000–4000 | 4000–8000 |
|---|---|---|---|---|---|---|
| caso_1 | 0,0 | 44,0 | 13,3 | 4,0 | **38,7** | 0,0 |
| caso_2 | 1,5 | 37,7 | 13,5 | 9,5 | 12,9 | **25,0** |
| caso_3 | 0,0 | 47,9 | 24,7 | **27,3** | 0,0 | 0,0 |

Las tres firmas son distintas y se leen directo de la tabla. En el caso 1 casi
el 39 % de la energía está en una banda donde la señal útil no tiene nada. En el
caso 2 la energía se reparte por todo el espectro, incluida la mitad superior
donde no hay señal útil. En el caso 3 la energía sobrante está pegada a la banda
útil, y por eso es el caso difícil.

La fase lo muestra todavía más claro que la magnitud. En el caso 1 la fase de
los tres tonos útiles se conserva intacta y solo aparece un punto nuevo en
3500 Hz. En el caso 3 la fase es azar en toda la banda 800–1500 Hz: ahí la
información original ya se perdió.

## d) Cómo identificar el ruido automáticamente

Tres características, medidas sobre los propios casos:

**Planitud espectral** (media geométrica entre media aritmética de la densidad
de potencia). Vale 1 para ruido blanco perfecto y tiende a cero cuando la
energía se concentra en pocos bines.

| | señal limpia | ruido solo | señal contaminada |
|---|---|---|---|
| caso_1 | 2,7·10⁻¹⁵ | 1,2·10⁻¹⁹ | 1,9·10⁻¹⁴ |
| caso_2 | 8,3·10⁻¹⁵ | **0,556** | **0,277** |
| caso_3 | 4,4·10⁻¹⁵ | 3,2·10⁻⁸ | 1,1·10⁻⁸ |

Separa los tres casos sin ambigüedad y por varios órdenes de magnitud. Un umbral
en 10⁻³ ya distingue «ruido de banda ancha» de «interferencia concentrada».

**Razón entre el pico del espectro y su mediana**, en dB:

| caso_1 | caso_2 | caso_3 |
|---|---|---|
| 181 dB | 41 dB | 124 dB |

Cuando hay una interferencia tonal, un solo bin se dispara muy por encima de la
mediana. Con ruido blanco no hay ningún bin dominante.

**Continuidad de los bines que superan el umbral.** Una interferencia tonal
ocupa uno o dos bines contiguos; un ruido de banda ocupa un tramo largo y
contiguo; el ruido blanco no forma tramos. Contar la longitud del tramo más
largo por encima de la mediana distingue el caso 1 del caso 3, que son los dos
que tienen planitud baja.

Con eso el nodo espectral del proyecto puede escoger su estrategia solo: si la
planitud es alta, atenuación proporcional al piso de ruido estimado; si hay un
pico dominante, rechazo estrecho centrado en el bin detectado; si hay un tramo
contiguo, rechazo de banda con el ancho medido.

## e) Qué no se puede recuperar

Esta es la parte importante y se ve midiendo la SNR **tono por tono**, es decir
cuánta energía de ruido cae encima de cada componente útil:

| Tono útil | caso_1 | caso_2 | caso_3 |
|---|---|---|---|
| 440 Hz | sin ruido apreciable | 16,9 dB | sin ruido apreciable |
| 880 Hz | sin ruido apreciable | 12,1 dB | **4,2 dB** |
| 1320 Hz | sin ruido apreciable | 6,1 dB | **−0,9 dB** |

(«Sin ruido apreciable» quiere decir que lo único que se mide en esa banda es la
fuga de la ventana de análisis, más de 100 dB por debajo.)

En el caso 1 ningún tono útil comparte frecuencia con el ruido, así que un
filtro puede quitar la interferencia entera sin tocar la señal: la información
se recupera completa. En el caso 3 el tono de 1320 Hz tiene **menos energía que
el ruido que lo acompaña**. Ahí no hay filtro lineal invariante en el tiempo que
sirva, porque la operación que separaría señal de ruido tendría que distinguir
dos cosas que ocupan el mismo bin de la DFT, y la DFT no guarda nada más con qué
distinguirlas.

Lo que sí se conserva cuando señal y ruido comparten frecuencia:

- La **energía total** de la banda, pero no cuánto de esa energía era señal. En
  el caso 3 la banda de 1320 Hz tiene 2,15 veces la energía que tenía en la
  señal limpia; todo ese exceso es ruido, y quitarlo a ciegas se lleva señal
  también.
- La **fase promedio** solo si la señal todavía domina. Donde el ruido es más
  fuerte (1320 Hz en el caso 3), la fase medida ya no es la de la señal.

La consecuencia práctica para el punto 2.4 es que cualquier método espectral que
atenúe la banda 800–1500 Hz en el caso 3 va a degradar los tonos de 880 y
1320 Hz. No es un defecto del método: es el límite del problema. El único margen
real está en elegir cuánto se sacrifica de cada lado, y eso es precisamente lo
que el enunciado pide demostrar.

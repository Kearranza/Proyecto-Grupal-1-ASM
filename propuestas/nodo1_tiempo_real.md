# Propuesta: el nodo 1 a tiempo real

Para Luis, de Kevin. Es tu código: esto es una propuesta, no un cambio hecho. Va como
parche (`nodo1_tiempo_real.patch`) para que lo revises y decidas.

## El problema, medido en la mesa

```
computo_us ultimo=42597 max=42732 presupuesto=32000 carga=133.1%
desbordamientos=4250
```

Con N = 1024 el salto es de 512 muestras: 32 ms. El cómputo tarda 42,6 ms, así que el DMA
pisa búferes y se pierden saltos enteros. En una captura de 10 s llegan ~128 000 muestras de
160 000.

Lo grave es lo que eso le hace a la medición. Los saltos perdidos caen en lugares distintos
en cada captura, y el desfase entre la referencia y una repetición de P3 va saltando de a
~512 muestras:

```
0 s: -6    1 s: -19    2 s: -2067    3 s: -6152    4 s: -848 ...
```

Después del primer hueco ya no coinciden, y la campaña dio ΔSNR ≈ 0,6 dB y 1,5 dB con
15-21 % de energía útil para los dos métodos. Son números inválidos. El enlace no lo nota
(`bloques_perdidos = 0`) porque esos bloques nunca se mandan.

## Qué cambia el parche

### 1. Sin `double` en el camino caliente (`diagnostico.c`, `nodo1.c`)

El RP2040 no tiene FPU, y en soft-float `double` cuesta varias veces lo que `float`.
Por cada salto se hacía en `double`:

- `diagnostico_estimar`: acumulados de energía y un `log()` por cada bin de ruido (hasta 512).
- `curtosis_diferencia`: 1023 iteraciones con cinco multiplicaciones `double`.
- La media para quitar la continua en `nodo1_procesar_salto`.

Ahora todo va en `float` (`logf`, `expf`, `log10f`). Además:

- `hypotf` pasa a `sqrtf(re*re + im*im)`. `hypotf` se cuida de desbordes que con
  magnitudes ≤ 1024 no pueden pasar.
- El bucle del diagnóstico ya no llama dos veces a `es_util` por bin.

Compilado para Cortex-M0+, `diagnostico.o` y `nodo1.o` ya no referencian ninguna rutina
`__aeabi_d*` ni `log`/`exp`/`log10` en doble.

### 2. Si va atrasado, manda el salto sin analizarlo (`nodo1_pico_w.ino`, `nodo1.c/h`)

Hay una función nueva, `nodo1_deslizar()`, que es solo el paso 1 de `nodo1_procesar_salto`
(deslizar el bloque). El paso 1 se separó tal cual; `nodo1_procesar_salto` la llama primero.

En `loop()`, al tomar un búfer, si el otro ya está lleno el nodo va un salto atrasado. En
ese caso el salto se desliza y se manda, pero no se le hace FFT ni diagnóstico. El ESP32 le
pega a ese salto el último diagnóstico, que es lo que ya hacía.

**El efecto: las muestras llegan siempre continuas, y si falta tiempo lo que baja es la
tasa de diagnósticos, no la evidencia.** El ruido no cambia cada 32 ms, así que saltarse un
diagnóstico de vez en cuando no le hace daño a nadie.

`ESTADO` por USB ahora muestra `sin_analizar=` para ver cuántos se saltaron. La trama
ESTADO no cambia, así que el protocolo sigue igual.

## Verificado

- Las 124 pruebas en C siguen pasando (fft 39, espectral 22, nodo1 15, metricas 12,
  nodo3 11, iir 7, trama 7, enlace 5, regresión 3, diagnóstico 3). Incluye las de
  `test_nodo1` que clasifican tonal, ninguno, blanco e impulsivo con N = 512, 1024 y 2048.
- El sketch compila para `rp2040:rp2040:rpipicow` (core 6.3.2). Las globales usan 49 % de
  la RAM, igual que antes.

## No verificado: falta probarlo en la placa

No se midió cuánto baja `computo_us`. Para comprobarlo:

1. Aplicar el parche y cargar el sketch:
   ```
   git apply propuestas/nodo1_tiempo_real.patch
   ```
2. En el monitor serie de la Pico W, `ESTADO` varias veces. Lo que se busca:
   - `computo_us ... carga=` por debajo de 100 %.
   - Si sigue arriba, que `desbordamientos` no suba y `sin_analizar` sí: eso confirma que
     el respaldo funciona.
3. Una captura de 10 s con `registrar.py` tiene que traer ~160 000 muestras.

Si la carga sigue arriba del 100 %, la prueba más barata que queda es el compilador:
*Herramientas → Optimize* en `-O2` o `-O3` en vez de `-Os`. No toca código.

Para deshacer el parche sin commitear:
```
git apply -R propuestas/nodo1_tiempo_real.patch
```

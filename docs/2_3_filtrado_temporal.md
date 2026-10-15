# 2.3 Diseño de un filtro temporal IIR

Para este apartado se eligió el **caso 1** (interferencia sinusoidal de 3500 Hz separada de las componentes útiles de 440, 880 y 1320 Hz) y se diseñó un **filtro IIR elíptico de rechazo de banda** de orden 6, implementado como una cascada de tres secciones de segundo orden (biquads) en forma directa II transpuesta.

El código está en `src/filtros_temporales.py`. Las figuras son `fig_23_polos_ceros.png` y `fig_23_respuestas.png`, y las salidas filtradas de los tres casos quedan en `resultados/salidas/caso_N_temporal.wav`.

---

## a) Especificación

| Parámetro | Valor |
|---|---|
| Frecuencia de muestreo | 16 kHz |
| Interferencia a eliminar | 3500 Hz |
| Banda de rechazo | 3470 – 3530 Hz (±30 Hz) |
| Bandas de paso | 0 – 3400 Hz y 3600 – 8000 Hz |
| Frecuencias de borde | 3400, 3470, 3530 y 3600 Hz |
| Rizado máximo en la banda de paso | 1 dB |
| Atenuación mínima en la banda de rechazo | 40 dB |

Los tonos útiles (440, 880 y 1320 Hz) quedan a más de 2 kHz del borde de paso más cercano (3400 Hz), de modo que se encuentran en plena banda de paso, lejos de las transiciones. Esta condición se comprueba automáticamente en `test_la_banda_de_paso_no_invade_los_tonos_utiles`.

Con esas especificaciones, el filtro diseñado cumple:

- máximo de |H| en la banda de rechazo: −40.00 dB;
- mínimo de |H| en las bandas de paso: −1.000 dB;
- |H(3500 Hz)| = −89.4 dB.

---

## b) Justificación del tipo y del orden

**Tipo.** El espectro del caso 1 muestra una única interferencia tonal aislada, por lo que corresponde un filtro de rechazo de banda: elimina solo la región de la interferencia y deja pasar el resto del espectro. Un pasa bajas con corte alrededor de 2 kHz también eliminaría esta interferencia en el caso 1, pero descartaría todo el contenido por encima de ese corte, útil o no, y no se generaliza a una interferencia que cambie de posición.

**Familia y orden.** Para la misma especificación, los órdenes totales calculados con las funciones de orden de SciPy son:

| Filtro | Orden total |
|---|---|
| Butterworth | 10 |
| Chebyshev I | 8 |
| Chebyshev II | 8 |
| **Elíptico** | **6** |
| FIR (ventana Kaiser, transición de 70 Hz, 40 dB) | ≈ 512 coeficientes |

Se escogió el elíptico porque entrega el menor orden: 3 biquads, es decir, 15 multiplicaciones-acumulaciones por muestra, frente a unas 500 de un FIR equivalente. El costo del elíptico es que presenta rizado en ambas bandas (aceptado en la especificación) y una fase no lineal. Este último punto se discute en f).

---

## c) Coeficientes de la ecuación de diferencias

Cada sección k cumple

y[n] = b₀ x[n] + b₁ x[n−1] + b₂ x[n−2] − a₁ y[n−1] − a₂ y[n−2]

y se implementa en forma directa II transpuesta con dos estados (z₁, z₂):

```
y  = b0*x + z1
z1 = b1*x - a1*y + z2
z2 = b2*x - a2*y
```

| Sección | b₀ | b₁ | b₂ | a₁ | a₂ |
|---|---|---|---|---|---|
| 0 | 0.91308167 | −0.35630097 | 0.91308167 | −0.36307738 | 0.86089476 |
| 1 | 1.00000000 | −0.36230176 | 1.00000000 | −0.31232631 | 0.98234956 |
| 2 | 1.00000000 | −0.41797720 | 1.00000000 | −0.46016607 | 0.98261212 |

(con a₀ = 1 en las tres).

---

## d) Función de transferencia

H(z) = H₀(z) · H₁(z) · H₂(z), con

H₀(z) = (0.91308167 − 0.35630097 z⁻¹ + 0.91308167 z⁻²) / (1 − 0.36307738 z⁻¹ + 0.86089476 z⁻²)

H₁(z) = (1 − 0.36230176 z⁻¹ + z⁻²) / (1 − 0.31232631 z⁻¹ + 0.98234956 z⁻²)

H₂(z) = (1 − 0.41797720 z⁻¹ + z⁻²) / (1 − 0.46016607 z⁻¹ + 0.98261212 z⁻²)

Se trabajó con la forma factorizada y no con un único cociente de polinomios de grado 6, porque esa forma es numéricamente más robusta.

---

## e) Polos, ceros, región de convergencia y estabilidad

La Figura `fig_23_polos_ceros.png` muestra el plano z.

Los numeradores son simétricos (b₀ = b₂), por lo que los seis ceros están **sobre el círculo unitario**, en pares conjugados. Los polos quedan dentro, cerca de los ceros:

| Sección | Ceros (|z| = 1), frecuencia | Polos, radio | Polos, frecuencia |
|---|---|---|---|
| 0 | ≈ 3500 Hz | 0.9278 | ≈ 3499 Hz |
| 1 | ≈ 3536 Hz | 0.9911 | ≈ 3597 Hz |
| 2 | ≈ 3464 Hz | 0.9913 | ≈ 3403 Hz |

Esta disposición es la típica del rechazo de banda: los ceros anulan la banda de rechazo y los polos cercanos mantienen angosta la muesca.

- **Región de convergencia (sistema causal):** |z| > 0.991268, el radio del polo más externo.
- **Estabilidad:** como el radio polar máximo es 0.991268 < 1, la ROC contiene la circunferencia unitaria y el filtro es estable (BIBO).

El margen respecto al círculo unitario es pequeño (≈ 0.009), consecuencia de una muesca tan angosta. Eso hace relevante el estudio de cuantización del inciso h).

---

## f) Respuesta en magnitud, fase y retardo de grupo

La Figura `fig_23_respuestas.png` muestra magnitud, fase y retardo de grupo.

- **Magnitud:** plana dentro del rizado de 1 dB fuera de la muesca, con una caída por debajo de −40 dB en 3470–3530 Hz y un mínimo de −89.4 dB en 3500 Hz.
- **Fase:** no lineal. Cambia rápidamente alrededor de la muesca.
- **Retardo de grupo:**

| Frecuencia | Retardo de grupo |
|---|---|
| 440 Hz | 0.12 muestras (≈ 7 µs) |
| 880 Hz | 0.14 muestras (≈ 8 µs) |
| 1320 Hz | 0.17 muestras (≈ 11 µs) |
| 3000 Hz | 2.25 muestras (0.14 ms) |
| 3400 Hz | ≈ 123 muestras (7.7 ms) |
| 3500 Hz | ≈ 6.3 muestras (0.39 ms) |
| 3600 Hz | ≈ 122 muestras (7.6 ms) |

En los tonos útiles el retardo es despreciable. Crece mucho en las transiciones y alcanza picos del orden de 10³ muestras en un intervalo muy estrecho junto a un cero en ≈ 3464 Hz, es decir, en una región donde la señal ya está atenuada más de 40 dB. A diferencia de un FIR de fase lineal, un IIR no conserva la forma de onda de señales con contenido cercano a la muesca.

---

## Verificación contra la biblioteca

La ecuación de diferencias se aplicó con código propio (cascada de biquads en forma directa II transpuesta y, además, la forma directa I con los coeficientes b y a) y se comparó con `scipy.signal.sosfilt` y `scipy.signal.lfilter`. La tolerancia declarada es de **10⁻⁹** en error absoluto máximo (doble precisión).

| Caso | Error máx. (SOS vs `sosfilt`) | Error máx. (b, a vs `lfilter`) | Resultado |
|---|---|---|---|
| caso 1 | 0 | 2.81 × 10⁻¹³ | cumple |
| caso 2 | 0 | 1.86 × 10⁻¹³ | cumple |
| caso 3 | 0 | 1.90 × 10⁻¹³ | cumple |

El error exactamente nulo de la versión SOS es esperable: la implementación propia y `sosfilt` usan la misma forma y el mismo orden de operaciones. El error de ≈ 10⁻¹³ de la forma directa corresponde a redondeo de punto flotante.

---

## g) Aplicación a los tres casos

El filtro, optimizado para el caso 1, se aplicó a los tres casos. La SNR se midió respecto a la señal limpia, descartando las primeras 500 muestras (transitorio) y **sin compensar el retardo del filtro**.

| Caso | SNR antes (dB) | SNR después (dB) | Mejora (dB) |
|---|---|---|---|
| caso 1 (tonal, 3500 Hz) | 2.03 | 30.13 | +28.10 |
| caso 2 (ruido blanco) | 0.03 | 0.22 | +0.18 |
| caso 3 (banda 800–1500 Hz) | 3.01 | 3.02 | +0.00 |

- **Caso 1.** Es el caso para el que se diseñó el filtro: la interferencia se atenúa más de 40 dB y los tonos útiles pasan casi intactos. La SNR final de 30 dB incluye también el error de fase y de rizado sobre los tonos útiles, ya que no se alineó la salida con la referencia; no es solo ruido residual.
- **Caso 2.** La mejora es marginal. La muesca elimina unos 60 Hz de un ruido distribuido en 8000 Hz, menos del 1 % de su potencia.
- **Caso 3.** No hay mejora: el ruido ocupa 800–1500 Hz, lejos de la muesca, y el filtro no lo toca. Reducirlo exigiría un filtro en esa banda, que degradaría los tonos de 880 y 1320 Hz que comparten el espectro con el ruido.

El resultado confirma que un filtro temporal fijo funciona cuando señal y ruido están separados en frecuencia y que no se generaliza a ruidos de otra naturaleza.

---

## h) Cuantización de coeficientes, saturación y memoria

**Cuantización.** Se redondearon los coeficientes a punto fijo con distintas cantidades de bits fraccionarios y se comparó la respuesta con la ideal:

| Bits fraccionarios | Estable | Desv. máx. en paso (dB) | Máx. \|H\| en rechazo (dB) |
|---|---|---|---|
| 8 | sí | 0.977 | −39.57 |
| 12 | sí | 0.034 | −39.96 |
| 14 | sí | 0.010 | −39.99 |
| 16 | sí | 0.005 | −40.00 |
| float32 | sí | ≈ 0 | −40.00 |

Con 8 bits el rizado de paso casi se duplica y la atenuación queda por debajo de la especificación; con 12 bits o más la respuesta queda prácticamente intacta. El radio polar máximo permanece en ≈ 0.9913 en todos los casos, y la simetría b₀ = b₂ sobrevive al redondeo, así que los ceros siguen sobre el círculo unitario.

La desviación máxima global de la magnitud (30.4, 5.9, 4.1 y 1.3 dB para 8, 12, 14 y 16 bits) es mucho mayor que la de la banda de paso. Se debe a que ocurre en el fondo de la muesca, donde |H| ≈ −90 dB y un cambio mínimo de coeficientes produce una diferencia grande en dB sin importancia práctica.

**Saturación.** Con la salida del caso 1 escalada a Q1.14 (1.0 ↔ 16384), no hubo saturación en int16 (0 muestras), es decir, hay margen hasta ±2.0 de amplitud. En el diseño real hay que verificar además los estados internos, que pueden exceder el rango de la salida, y usar acumuladores de mayor ancho que los coeficientes.

**Memoria y costo para [microcontrolador].**

| Recurso | Valor |
|---|---|
| Coeficientes (15 × 4 B) | 60 B |
| Estados (6 × 4 B) | 24 B |
| Total | 84 B |
| Costo por muestra | 15 MAC |
| Costo a 16 kHz | 240 000 MAC/s |

Recomendación: con unidad de punto flotante, usar `float32` directamente. Sin ella, hay dos opciones: coeficientes de 32 bits en punto fijo (por ejemplo Q2.30) con acumulador de 64 bits, o coeficientes de 16 bits en formato Q2.14 (14 bits fraccionarios, rango ±2), que dan una desviación de solo 0.010 dB en la banda de paso. El formato Q1.15 no sirve porque |a₁| puede superar 1.
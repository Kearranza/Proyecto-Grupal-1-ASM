# Conexiones

| Desde | Hacia | Nota |
|---|---|---|
| Emisor (Pico de Santiago) GP15 → RC 2 etapas | Pico W GPIO26 (ADC0) | P1 y P2. RC de 470 Ω + 10 nF por etapa (taller) |
| Tarjeta de sonido de la PC → circuito de nivel | Pico W GPIO26 (ADC0) | P3–P6. Ver "Circuito de la tarjeta de sonido" |
| Pico W GP8 (TX UART1) | ESP32 GPIO16 (RX UART2) | 921 600 baud |
| Pico W GP9 (RX UART1) | ESP32 GPIO17 (TX UART2) | Reservado para comandos de vuelta |
| ESP32 GPIO25 (DAC1) → filtro de reconstrucción | Salida del nodo 2 (temporal) | RC de 2 etapas, 1 kΩ + 10 nF. Ver "Filtro de reconstrucción" |
| ESP32 GPIO26 (DAC2) → filtro de reconstrucción | Salida del nodo 3 (espectral) | Mismo filtro que DAC1, para que las salidas sean comparables |
| GND de todas las placas y de la PC | común | Sin tierra común la UART y el ADC fallan |

Las dos entradas al ADC (emisor y tarjeta de sonido) no se conectan a la vez: se usa una
u otra según la prueba.

## Circuito de la tarjeta de sonido

La salida de audio de la PC está centrada en 0 V y puede pasar de ±1 V. El ADC de la
Pico W acepta 0 a 3,3 V. El circuito hace tres cosas: bloquea la componente continua
(capacitor de acople), centra la señal en 1,65 V (divisor) y reduce la amplitud a la mitad
aproximadamente para que una salida normal no salga del rango del ADC (requisito 5.7: no
dañar entradas analógicas).

```
                                        3V3 (pin 36)
                                           │
                                         [4,7 kΩ]
                                           │
 PC audio ──[10 µF]──[2,2 kΩ]──────────────┼────[1 kΩ]──── GPIO26 (ADC0)
 (punta)   (+ hacia                        │
            el divisor)                  [4,7 kΩ]
                                           │
 PC audio (masa) ──────────────────────── GND (común con la Pico W)
```

| Componente | Valor | Función |
|---|---|---|
| C1 | 10 µF (electrolítico, **+ del lado del divisor**) | Acople: quita el continuo de la PC. Corte pasa altas ≈ 3,5 Hz |
| R1 | 2,2 kΩ | Serie: junto con el divisor fija la ganancia y limita la corriente |
| R2, R3 | 4,7 kΩ y 4,7 kΩ | Divisor 3V3 / GND: centra la señal en 1,65 V (Thevenin 2,35 kΩ) |
| R4 | 1 kΩ | Protección del pin del ADC contra picos |

Valores resultantes (calculados):

| Magnitud | Valor |
|---|---|
| Ganancia de pequeña señal | 2,35 / (2,2 + 2,35) = **0,516** |
| Impedancia de entrada vista por la PC | 4,55 kΩ (la tarjeta de sonido la maneja sin problema) |
| Impedancia de la fuente vista por el ADC | ≈ 1,1 kΩ (+ R4) |
| Nivel de 1 Vrms (línea, 0 dBV) | ±1,41 V pico → ±0,73 V en el ADC → de 0,92 V a 2,38 V |
| Pico máximo de la PC antes de recortar | ±3,19 V |

Con esa ganancia, una señal de audio normal usa cerca de la mitad del rango del ADC, que
deja margen para picos. En la práctica:

1. **Tierra común.** La masa del audio debe ir al GND de la Pico W. Un bucle de tierra con
   la PC puede meter zumbido de 60 Hz, que se confundiría con el ruido que se quiere
   medir. Si aparece, alimentar la PC con el cargador desconectado de la pared o usar un
   aislador de audio.
2. **Volumen de la PC.** Empezar con el volumen al 30 % y subir mientras se mira la señal
   en el nodo del ADC con el osciloscopio. El pico no debe pasar de ±1,2 V alrededor de
   1,65 V (de 0,45 V a 2,85 V).
3. **Protección.** Si se usa la salida de auriculares, que puede dar varios voltios, agregar
   dos diodos Schottky (por ejemplo 1N5819) en el nodo del ADC: uno a 3V3 y otro a GND.
4. **Calibración de nivel.** Reproducir un seno de 1 kHz a amplitud conocida, medir su Vpp en
   el nodo del ADC y anotar el factor entre la amplitud del WAV y los voltios. Las
   métricas de la campaña (SNR, MSE normalizado) son relativas y no dependen de ese
   factor, pero sí hay que cuidar que no se recorte.
5. **Verificación sin señal.** Con la PC en silencio, la lectura del ADC debe estar en
   torno a 2048 cuentas (1,65 V). Si no, el divisor está mal conectado.

## Filtro de reconstrucción de los DAC del ESP32

Los DAC del ESP32 son de 8 bits y se actualizan a fs = 16 kHz (retención de orden cero).
Eso deja copias de la señal alrededor de 16 kHz ± f, que hay que atenuar. El filtro es un
RC pasa bajas de dos etapas, igual en DAC1 y DAC2:

```
 GPIO25 (DAC1) ──[1 kΩ]──┬──[1 kΩ]──┬──── salida nodo 2 (temporal)
                         │          │
                       [10 nF]    [10 nF]
                         │          │
 GND ────────────────────┴──────────┴──── GND

 GPIO26 (DAC2): mismo circuito → salida nodo 3 (espectral)
```

Con la carga de la segunda etapa, la respuesta es H(f) = 1 / (1 − x² + 3jx), con
x = 2π f R C. Los capacitores de 10 nF son los marcados "103". Ganancia total, incluyendo el
efecto de la retención de orden cero:

| R y C por etapa | f₀ | −3 dB | 440 Hz | 1320 Hz | 3500 Hz | Copia de 3500 Hz (12,5 kHz) |
|---|---|---|---|---|---|---|
| 470 Ω + 10 nF | 33,9 kHz | 12,7 kHz | −0,02 dB | −0,14 dB | −1,01 dB | −14,7 dB |
| **1 kΩ + 10 nF** | **15,9 kHz** | **6,0 kHz** | **−0,03 dB** | **−0,30 dB** | **−1,97 dB** | **−19,3 dB** |
| 1,5 kΩ + 10 nF | 10,6 kHz | 4,0 kHz | −0,06 dB | −0,55 dB | −3,18 dB | −22,8 dB |

Se elige **1 kΩ + 10 nF** por este compromiso: los tonos de 440 a 1320 Hz casi no se
alteran (menos de 0,3 dB), las copias quedan unos 19 dB abajo y el filtro es el mismo
para las dos salidas. La pérdida de unos 2 dB en 3500 Hz es esperable y no importa
para la comparación, porque es la frecuencia de la interferencia que se quiere eliminar.
Como las dos salidas usan el mismo filtro, esa pérdida afecta por igual a ambas.

Notas:

- La salida del DAC está centrada en 1,65 V (la señal va montada sobre un continuo). Para
  escucharla, agregar un capacitor de 10 µF en serie y conectarla a un amplificador o
  parlante activo, nunca directamente a unos auriculares.
- La salida del DAC del ESP32 admite poca corriente. Cargarla con una entrada de alta
  impedancia (osciloscopio de 1 MΩ o entrada de un amplificador) y verificar la forma
  de onda antes de conectar otra cosa.
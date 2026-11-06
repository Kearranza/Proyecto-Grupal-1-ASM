# Conexiones

| Desde | Hacia | Nota |
|---|---|---|
| Emisor (Pico de Santiago) GP15 → RC 2 etapas | Pico W GPIO26 (ADC0) | P1 y P2. RC de 470 Ω + 10 nF por etapa (taller) |
| Tarjeta de sonido de la PC → circuito de nivel | Pico W GPIO26 (ADC0) | P3–P6. TODO (Santiago): diagrama y valores |
| Pico W GP8 (TX UART1) | ESP32 GPIO16 (RX UART2) | 921 600 baud |
| Pico W GP9 (RX UART1) | ESP32 GPIO17 (TX UART2) | Reservado para comandos de vuelta |
| ESP32 GPIO25 (DAC1) → filtro de reconstrucción | Salida del nodo 2 (temporal) | TODO (Santiago): RC y su justificación |
| ESP32 GPIO26 (DAC2) → filtro de reconstrucción | Salida del nodo 3 (espectral) | Mismo filtro que DAC1, para que las salidas sean comparables |
| GND de todas las placas y de la PC | común | Sin tierra común la UART y el ADC fallan |

## Circuito de la tarjeta de sonido (TODO Santiago)

La salida de audio de la PC está centrada en 0 V y puede pasar de ±1 V. El ADC de la
Pico W acepta 0 a 3,3 V. Hace falta un capacitor de acople, un divisor que centre la
señal en 1,65 V y limitar la amplitud para no salir del rango (requisito 5.7: no dañar
entradas analógicas). Documentar valores y la ganancia resultante, que la campaña
tiene que tomar en cuenta.

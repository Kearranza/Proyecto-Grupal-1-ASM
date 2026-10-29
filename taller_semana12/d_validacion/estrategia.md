# Bloque D: estrategia de validación en el receptor (rama `taller-validacion`)

Punto 3 del taller: **diagramas** que muestren cómo se va a comprobar que la Pico W
(nodo 1) recibe bien lo que manda la Pico emisora. El taller no pide que ya funcione,
solo la estrategia. Pero los diagramas tienen que ser concretos: con pines,
tamaños, umbrales y qué pasa cuando algo falla.

## Diagramas mínimos (fuentes en `diagramas/`, se renderizan con `renderizar.sh`)

| Archivo | Qué muestra | Estado |
|---|---|---|
| `01_cadena_senal.mmd` | Pico (PWM) → RC → ADC de la Pico → nodo 1, con niveles y fs | hecho |
| `02_flujo_receptor.mmd` | Captura de un bloque: ADC → búfer → quitar DC → ventana → FFT → comparar | hecho |
| `03_trama.mmd` | Cómo manda el nodo 1 sus resultados: inicio, número de bloque, modo, picos, CRC-16 | hecho |
| `04_decision.mmd` | Qué hace el receptor si hay CRC malo, bloque perdido o búfer lleno | hecho |

## Lo que cada diagrama debe responder

- **Cadena de señal:** ¿qué voltaje mínimo/máximo llega al ADC? ¿hay riesgo de pasar
  3,3 V? ¿dónde está el filtro antialiasing?
- **Flujo del receptor:** ¿cada cuánto se llena un bloque de N muestras? ¿con qué se
  compara la FFT del micro (la referencia es `senal_del_modo()` del bloque A)?
- **Trama:** ¿cómo se sabe dónde empieza un bloque (delimitación)? ¿cómo se detecta
  un error (CRC-16 o checksum)? ¿cómo se nota un bloque perdido (número de secuencia)?
  Son requisitos 5.2 y 5.3 del proyecto.
- **Decisión:** ¿qué se muestra o se registra en cada falla?

## Criterios de aceptación (llenar en `parametros.json → validacion`)

El proyecto pide fijarlos **antes** de medir (sección 3.5). Proponer y justificar:

- `margen_frecuencia_bins`: cuánto se puede correr un pico (en bins de fs/N).
- `margen_amplitud_db`: diferencia aceptable de nivel contra la referencia.
- `snr_minima_captura_db`: SNR mínima de la captura limpia (modo 1) para considerar
  que la cadena PWM → ADC no está metiendo ruido de más.

## Entrega en el PDF (`informe/secciones/4_validacion.tex`)

Los cuatro diagramas, la tabla de criterios con su justificación y un párrafo de
qué se haría si una prueba falla. D además integra el PDF (`informe/taller.tex`).

## Decisiones tomadas

- **Filtro RC:** dos etapas de R = 1,5 kΩ y C = 10 nF, fc = 10,6 kHz. Atenúa la
  portadora de 488 kHz unos 66 dB y solo 0,9 dB en 3,5 kHz. Es una **propuesta de D**
  para poder poner números en el diagrama 01; **C la confirma al medir**.
- **El RC no es filtro antialias.** Su fc queda por encima de Nyquist (8 kHz). Sirve
  igual porque el emisor no genera nada entre 3,5 kHz y la portadora. Cuando el nodo 1
  tome una señal de una fuente arbitraria va a necesitar un antialias de verdad antes
  del ADC.
- **La trama lleva solo los picos, no los 513 bins.** Validar necesita el bin y el
  nivel de cada componente. Son 8 + 4K bytes (24 B con K = 4, o 750 B/s) contra
  1026 B por bloque (32 kB/s) si se mandara el espectro completo.

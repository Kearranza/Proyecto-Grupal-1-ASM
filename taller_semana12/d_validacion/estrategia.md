# Bloque D: estrategia de validación en el receptor (rama `taller-validacion`)

Punto 3 del taller: **diagramas** que muestren cómo se va a comprobar que la Pico W
(nodo 1) recibe bien lo que manda la Pico emisora. El taller no pide que ya funcione,
solo la estrategia. Pero los diagramas tienen que ser concretos: con pines,
tamaños, umbrales y qué pasa cuando algo falla.

## Diagramas mínimos (fuentes en `diagramas/`, se renderizan con `renderizar.sh`)

| Archivo | Qué muestra | Estado |
|---|---|---|
| `01_cadena_senal.mmd` | Pico (PWM) → RC → ADC de la Pico → nodo 1, con niveles y fs | base hecha, revisar |
| `02_flujo_receptor.mmd` | Captura de un bloque: ADC → búfer → quitar DC → ventana → FFT → comparar | por hacer |
| `03_trama.mmd` | Cómo manda el nodo 1 sus resultados: inicio, número de bloque, largo, datos, CRC | por hacer |
| `04_decision.mmd` | Qué hace el receptor si hay CRC malo, bloque perdido o búfer lleno | por hacer |

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

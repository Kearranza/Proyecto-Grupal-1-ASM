# Firmware

Esta carpeta **es** un sketchbook de Arduino. No hay que copiar nada a mano.

## Una sola vez, en el IDE

Archivo -> Preferencias -> "Ubicacion del sketchbook", y apuntarla a esta carpeta:

    C:\Users\<usuario>\Desktop\Taller\Proyecto-Grupal-1-ASM\proyecto\firmware

Con eso pasan dos cosas: el IDE encuentra `libraries/asm_comun` solo, y los tres sketches
aparecen en Archivo -> Sketchbook. Sin esto ninguno compila, porque todos hacen
`#include <asm_comun.h>` y esa libreria vive aqui, no en la carpeta Arduino del usuario.

Si se prefiere no mover el sketchbook, la alternativa es copiar la carpeta
`libraries/asm_comun` a `Documentos\Arduino\libraries\`. Funciona, pero queda una copia
que hay que volver a copiar cada vez que alguien cambia un `.c`, asi que es peor.

## Los tres sketches

| Sketch | Placa | Que hace |
|---|---|---|
| `nodo1_pico_w/` | Pico W | Nodo 1: ADC, ventana, FFT, diagnostico y armado de tramas |
| `nodos23_esp32/` | ESP32 | Nodos 2 y 3: filtro temporal y espectral, metricas, DAC |
| `sonda_emisor/` | Pico | Emite la senal de prueba por PWM y escucha el enlace para `pc/registrar.py` |

## Placas que hay que tener instaladas

- Pico y Pico W: gestor de tarjetas `arduino-pico` de Earle Philhower
- ESP32: `esp32` de Espressif

## Antes de cargar

`config_generado.h` sale de `config/sistema.json`. Si alguien cambia ese JSON hay que
volver a correr `python proyecto/pc/generar_config_h.py` y commitear los dos archivos, o
las placas quedan con numeros distintos a los de la computadora.

La tabla de `sonda_emisor/tabla_senal.h` la genera `python proyecto/pc/tabla_senal.py p3`
(o `p4`, `p5`). Hay que regenerarla al cambiar de prueba.

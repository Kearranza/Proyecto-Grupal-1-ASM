# Proyecto Grupal 1: sistema distribuido de reducción de ruido

Entrega y defensa: **viernes 6 de noviembre de 2026** (archivos hasta las 11:50 p. m.).
La estructura del repo no cambia: todo lo del proyecto vive en esta carpeta y reutiliza
`src/` (Tarea 1) y `metadatos/`.

## Bloques

| Bloque | Responsable | Rama | Archivos |
|---|---|---|---|
| Nodo 1 + base del ESP32 | Luis | `proyecto-nodo1` | `firmware/nodo1_pico_w/`, `firmware/nodos23_esp32/` (recepción, tareas, DAC), `diagnostico.c`, `paper/secciones/3_arquitectura.tex` |
| Nodo 2 temporal + fuente de señal | Santiago | `proyecto-temporal` | `iir.c`, `pc/generar_wav_pruebas.py`, `senales/`, `docs/conexiones.md`, `paper/secciones/4_temporal.tex` |
| Nodo 3 espectral | Gretchell | `proyecto-espectral` | `fft.c` (compartido), `espectral.c`, `metricas.c`, interfaz del nodo 3, `paper/secciones/2_teoria.tex` y `5_espectral.tex` |
| Protocolo, pruebas y paper | Kevin | `proyecto-protocolo` | `trama.c` (compartido), `pc/trama.py`, `pc/registrar.py`, `pc/comandos.py`, `pc/campana.py`, `config/pruebas.json`, `docs/protocolo.md`, `paper/` (integración, 0, 1, 6, 7) |

Los `.c` están en `firmware/libraries/asm_comun/src/`. Son **C puro**: compilan igual en la
Pico W, en el ESP32 y en la computadora con `gcc`, así que todos avanzan sin placa.

## Hardware

| Función | Placa | Notas |
|---|---|---|
| Nodo 1 | Pico W (Luis) | ADC0 en GPIO26, UART1 TX en GP8 |
| Nodos 2 y 3 | ESP32 clásico (Luis) | Un núcleo por método; DAC1 (GPIO25) temporal, DAC2 (GPIO26) espectral |
| Emisor P1/P2 | Pico (Santiago) | `taller_semana12/c_emisor/` |
| Emisor P3–P6 | PC reproduciendo WAV | Pendiente confirmar con el profesor |

Conexiones completas en `docs/conexiones.md`.

## Decisiones acordadas

| Decisión | Valor | Por qué (resumen) |
|---|---|---|
| Lenguaje | C/C++ con Arduino | 62,5 µs por muestra no se cumplen en MicroPython; módulos probables en la PC |
| Señal a nodos 2 y 3 | El nodo 1 manda las muestras | Adquisición separada (enunciado); ADC del ESP32 menos lineal |
| Enlace | UART 921 600 baud, 8N1 | 32 kB/s = 320 kbit/s; margen 2,9× |
| Muestras en el cable | int16 little-endian | 12 bits caben; float duplica el tráfico |
| Trama | `AA 55 | tipo | seq | largo | carga | CRC-16` | Pérdidas (5.3) y errores (5.2); N variable |
| Números en el ESP32 | float32 | Tiene FPU para float; ESP-DSP; IIR sensible a cuantizar |
| Números en la Pico W | float32, medir primero | Sin FPU: ~20 ms por FFT de 1024 contra 32 ms disponibles |
| Tamaño de bloque | 256–2048, por comando | El profe puede cambiar N; búferes para 2048 |

Todos los números están en `config/sistema.json`. Para pasarlos a C:
`python proyecto/pc/generar_config_h.py` (escribe `config_generado.h`; commitear los dos).

## Interfaces compartidas (no cambiarlas sin avisar)

```c
int fft(float *re, float *im, int n);                 // Gretchell
int ifft(float *re, float *im, int n);
int trama_armar(uint8_t tipo, uint16_t seq, const uint8_t *carga, uint16_t largo,
                uint8_t *salida, size_t capacidad);     // Kevin
int trama_leer_byte(lector_trama_t *l, uint8_t b, trama_t *salida);
int diagnostico_estimar(const float *magnitud, int n, float fs, diagnostico_t *d);  // Luis
int iir_disenar(iir_t *f, const diagnostico_t *d, float fs);                        // Santiago
int iir_procesar(iir_t *f, const float *x, float *y, int n);
int espectral_procesar_salto(espectral_t *e, const float *nuevas, float *salida,
                             const diagnostico_t *d, float fs);                     // Gretchell
static inline void salida_dac(int canal, float muestra);                            // Luis (ESP32)
```

Frontera del ruido: el nodo 1 **estima** (`diagnostico_t`), el nodo 2 **ajusta su filtro**
con eso y el nodo 3 **decide qué bins tocar**. Nadie más vuelve a clasificar.

Todas devuelven `ASM_OK`, `ASM_ERROR` o `ASM_PENDIENTE` (mientras no estén hechas).

## Cómo probar

Desde la raíz del repo:

```bash
python proyecto/pc/generar_config_h.py     # si cambió config/sistema.json
python proyecto/pc/referencias_c.py        # datos de referencia desde el código de la Tarea 1
make -C proyecto/pruebas_c test            # pruebas de todos los módulos en C
python proyecto/pc/trama.py                # autoprueba del protocolo en Python
python proyecto/pc/campana.py              # estado de la campaña de pruebas
```

En Windows sin `make`: usar WSL, o compilar una prueba a mano desde `proyecto/pruebas_c`:
`gcc -I../firmware/libraries/asm_comun/src test_fft.c ../firmware/libraries/asm_comun/src/*.c -lm -o test_fft`.

Cada prueba sale como `[ok]`, `[PENDIENTE]` o `[FALLA]`. Al inicio todo está pendiente.
Solo las fallas hacen fallar `make test`. Los umbrales marcados como PROVISIONALES en
`test_iir.c` y `test_espectral.c` los ajusta su responsable y los justifica en el paper.

**Ojo, nodo 1:** `clasificar_ruido()` de la Tarea 1 marca `caso_3` (banda) como tonal y en
`caso_1` toma el tono útil de 440 Hz como interferencia, porque no excluye las bandas
útiles. Por eso `test_diagnostico.c` compara contra la verdad de cada caso, no contra la
Tarea 1. El diagnóstico tiene que mejorarse, no solo portarse.

## Cómo compilar el firmware

Cores: "Raspberry Pi Pico/RP2040" (Earle Philhower) y "esp32" de Espressif 3.x.

- **Arduino IDE 2:** Archivo → Preferencias → *Ubicación del sketchbook* =
  `<repo>/proyecto/firmware`. Así el IDE encuentra los sketches y la librería `asm_comun`.
- **arduino-cli:**
  ```bash
  arduino-cli compile --libraries proyecto/firmware/libraries -b rp2040:rp2040:rpipicow proyecto/firmware/nodo1_pico_w
  arduino-cli compile --libraries proyecto/firmware/libraries -b esp32:esp32:esp32 proyecto/firmware/nodos23_esp32
  ```

Los sketches pasaron una revisión de sintaxis, pero **no se han compilado con los cores
reales**: es lo primero que hay que hacer en cada placa.

## Paper

`paper/paper.tex`, formato IEEE Transactions, **máximo 4 páginas con referencias**. Cada
quien escribe solo su archivo de `paper/secciones/` (el comentario de arriba dice cuánto
espacio tiene). La teoría va una sola vez en `2_teoria.tex`; las demás secciones la citan.
Compilar en Overleaf o con pdfLaTeX + BibTeX. Lo que falte aparece como **[PENDIENTE]**.

## Git

Cada quien en su rama, creada desde `dev`; merge a `dev` con `--no-ff`; a `main` solo la
entrega con tag. Commits pequeños y frecuentes: el profesor revisa las estadísticas por
persona. Cada quien toca **solo sus archivos**. `config/sistema.json` y los `.h`
compartidos se cambian en un commit aparte y avisando al grupo.

## Calendario

| Día | Meta |
|---|---|
| Lun 2 | Estructura base en `dev`, ramas creadas |
| Mar 3 | `fft.c` y `trama.c` pasan sus pruebas; nodo 1 manda tramas; criterios de aceptación fijados |
| Mié 4 | Integración presencial en las placas |
| Jue 5 | Campaña P1–P5 × 3, figuras, borrador del paper |
| Vie 6 | Ensayo de P6 (cambiar parámetros en vivo), paper final, tag y defensa |

## Checklist

- [ ] `make -C proyecto/pruebas_c test` sin pendientes ni fallas
- [ ] Los dos sketches compilan y corren en sus placas
- [ ] Enlace Pico W → ESP32 sin errores de CRC ni bloques perdidos durante 1 minuto
- [ ] Las dos salidas DAC suenan y se ven en el osciloscopio
- [ ] Criterios de aceptación fijados en `config/pruebas.json` antes de medir
- [ ] P1–P5 × 3 medidas, con promedio y variabilidad
- [ ] N, umbral y método se cambian por comando sin recompilar (P6)
- [ ] Paper ≤ 4 páginas, sin [PENDIENTE]
- [ ] Merge a `main` y tag de entrega

# Tarea 1 — Caracterización espectral y prototipado de filtros para reducción de ruido

CE 1110 — Análisis de Señales Mixtas
Escuela de Ingeniería en Computadores, Instituto Tecnológico de Costa Rica
Profesor: Luis Chavarría Zamora

Primer avance técnico del Proyecto Grupal 1, *Diseño de un sistema distribuido de
reducción adaptativa de ruido en tiempo real*.

## Repositorio

- URL: https://github.com/Kearranza/Proyecto-Grupal-1-ASM
- Etiqueta de la entrega: `tarea1_prototipo`

## Integrantes

| Integrante | Carné | Bloque |
|---|---|---|
| Kevin Carranza Blanco | — | señales (2.2), comparación (2.5), informe |
| *(por completar)* | | fundamentos y FFT (2.1) |
| *(por completar)* | | filtro temporal (2.3) |
| *(por completar)* | | filtrado espectral (2.4) |

## Estructura

```
.
├── notebook.ipynb            # notebook ejecutable: regenera señales, figuras y métricas
├── src/
│   ├── dft_fft.py            # DFT por definición, DFT matricial y FFT radix-2 propias
│   ├── senales.py            # generación reproducible de los tres casos
│   ├── metricas.py           # MSE, SNR, ΔSNR, energía por bandas
│   ├── graficas.py           # figuras del punto 2.2
│   ├── filtros_temporales.py # filtro FIR/IIR (bloque 3)
│   └── filtrado_espectral.py # reducción por FFT/IFFT (bloque 4)
├── pruebas/
│   ├── caso_1/               # limpia.wav, ruido.wav, contaminada.wav y caso_1.json
│   ├── caso_2/
│   ├── caso_3/
│   └── test_senales.py       # pruebas automáticas de la generación
├── resultados/
│   ├── figuras/              # figuras del informe, con prefijo del punto
│   ├── salidas/              # señales ya filtradas por cada método
│   └── metricas.csv          # tabla comparativa final
├── metadatos/
│   └── configuraciones.json  # parámetros y semillas de los tres casos
├── docs/                     # enunciados, generador del profesor y secciones del informe
└── informe/                  # fuentes LaTeX del informe
```

## Los tres casos

La señal útil es la misma en los tres (multitono de 440, 880 y 1320 Hz) para que
cualquier diferencia medida se le pueda atribuir al ruido.

| Caso | Ruido | SNR objetivo | Para qué sirve |
|---|---|---|---|
| `caso_1` | tonal en 3500 Hz | 2 dB | interferencia bien separada de la banda útil |
| `caso_2` | blanco | 0 dB | ruido repartido por todo el espectro |
| `caso_3` | banda 800–1500 Hz | 3 dB | el ruido se solapa con dos de los tres tonos |

Los WAV de `pruebas/caso_*/` son **la entrada de todos los bloques**. Nadie
necesita generarlos: ya están en el repositorio.

## Reproducir

```bash
pip install numpy matplotlib
jupyter notebook notebook.ipynb
```

Ejecutar el notebook de arriba hacia abajo regenera desde cero las señales de
los tres casos, las figuras de `resultados/figuras/` y la tabla
`resultados/metricas.csv`. Todos los parámetros y semillas salen de
`metadatos/configuraciones.json`; no hay celdas que dependan de archivos no
versionados.

Las piezas también corren solas, sin el notebook:

```bash
python src/senales.py          # regenera los nueve WAV y los JSON de cada caso
python src/graficas.py         # regenera las figuras del punto 2.2
python pruebas/test_senales.py # 11 pruebas automáticas de la generación
```

## Ramas

| Rama | Contenido |
|---|---|
| `main` | solo entregas etiquetadas |
| `dev` | integración: aquí se juntan todos los bloques |
| `senales` | generación y análisis de señales (2.2) |
| `filtrado-temporal` | filtro FIR/IIR (2.3) |
| `filtrado-espectral` | reducción por FFT/IFFT (2.4) |
| `pruebas` | fundamentos y FFT (2.1) y pruebas automáticas |

Cada bloque sale de `dev` y vuelve a `dev`. A `main` solo se mergea la entrega.

## Estado

- [ ] 2.1 Fundamentos, muestreo y FFT
- [x] 2.2 Generación y caracterización de señales — `docs/2_2_senales.md`
- [ ] 2.3 Filtro temporal FIR o IIR
- [ ] 2.4 Reducción espectral FFT/IFFT
- [ ] 2.5 Comparación y propuesta embebida — necesita `resultados/salidas/`
- [ ] Informe PDF

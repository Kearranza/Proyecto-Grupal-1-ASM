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

| Integrante | Carné | Responsabilidad principal |
|---|---|---|
| Kevin Carranza Blanco | — | — |
| *(por completar)* | | |
| *(por completar)* | | |

## Estructura

```
.
├── notebook.ipynb            # notebook ejecutable: regenera señales, figuras y métricas
├── src/
│   ├── dft_fft.py            # DFT por definición, DFT matricial y FFT radix-2 propias
│   ├── senales.py            # generación reproducible de los tres casos de prueba
│   ├── metricas.py           # MSE, SNR, ΔSNR, energía por bandas
│   ├── filtros_temporales.py # diseño y aplicación del filtro FIR/IIR
│   └── filtrado_espectral.py # reducción de ruido por bloques con FFT/IFFT
├── pruebas/
│   ├── caso_1/               # WAV limpia, ruido y contaminada + metadatos del caso
│   ├── caso_2/
│   └── caso_3/
├── resultados/
│   ├── metricas.csv          # tabla comparativa de los tres casos
│   └── figuras/              # figuras del informe, regeneradas por el notebook
├── metadatos/
│   └── configuraciones.json  # parámetros y semillas de los tres casos
├── informe/                  # fuentes LaTeX del informe
└── docs/                     # enunciados y notebook generador del profesor
```

## Reproducir

```bash
pip install numpy scipy matplotlib
jupyter notebook notebook.ipynb
```

Ejecutar el notebook de arriba hacia abajo regenera desde cero las señales de los
tres casos, las figuras de `resultados/figuras/` y la tabla `resultados/metricas.csv`.
Todos los parámetros y semillas salen de `metadatos/configuraciones.json`; no hay
celdas que dependan de archivos no versionados.

## Ramas

| Rama | Contenido |
|---|---|
| `main` | integración de la entrega |
| `senales` | generación y análisis de señales (2.1 y 2.2) |
| `filtrado-temporal` | filtro FIR/IIR (2.3) |
| `filtrado-espectral` | reducción por FFT/IFFT (2.4) |
| `pruebas` | comparación automática entre implementaciones |

## Estado

- [ ] 2.1 Fundamentos, muestreo y FFT
- [ ] 2.2 Generación y caracterización de señales
- [ ] 2.3 Filtro temporal FIR o IIR
- [ ] 2.4 Reducción espectral FFT/IFFT
- [ ] 2.5 Comparación y propuesta embebida
- [ ] Informe PDF

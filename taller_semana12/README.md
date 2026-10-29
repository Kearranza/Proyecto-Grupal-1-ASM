# Taller semana 12: FFT y sistemas de modulación

Entrega: **viernes 30 de octubre, 11:59 p. m.**, por TEC-Digital (un .zip con el PDF y
los programas documentados). La defensa (entregable 3) no va en esta entrega.

El taller pide tres cosas, que se vuelven cuatro bloques independientes:

| Bloque | Responsable | Rama | Carpeta | Qué hace | Necesita placa |
|---|---|---|---|---|---|
| **A**: señales y frecuencias | Luis | `taller-senales` | `a_senales/` | Justifica fs y N, define la señal exacta de cada modo del emisor (la referencia de todos) y hace DFT vs FFT | No |
| **B**: sistema completo | Gretchell | `taller-sistema` | `b_sistema/` | Une emisor → PWM/ADC → nodo 1 → filtros → métricas en Python, reutilizando `src/` de la Tarea 1 | No |
| **C**: emisor en el micro | Santiago (propuesta) | `taller-emisor` | `c_emisor/` | La Pico genera los modos por PWM + RC (DDS), se cambian por serial y se verifica con FFT | **Sí** (Pico) |
| **D**: validación + PDF | por definir | `taller-validacion` | `d_validacion/` | Diagramas de cómo la Pico W valida lo que recibe, criterios de aceptación e integración del PDF | No |

## Hardware del grupo

| Persona | Placas | Uso en el taller | Uso posible en el proyecto |
|---|---|---|---|
| Luis | ESP32, Pico W | Pico W = receptor en los diagramas de D | Pico W = nodo 1 (adquisición); ESP32 = nodos 2 y 3 (doble núcleo, DAC para la salida del filtro temporal) |
| Santiago | Pico | Emisor (bloque C) | Emisor de las señales de prueba |
| Kevin | Arduino UNO | No hace falta | Respaldo del emisor. No sirve como nodo de FFT: tiene 2 KB de RAM y un bloque de 1024 muestras no cabe |
| Gretchell | Raspberry Pi | No hace falta | Si es una Pi 4/5 es una computadora, no un micro: sirve para visualizar y validar, no como nodo |

El emisor usa el core de Arduino "Raspberry Pi Pico/RP2040" (Earle Philhower). La Pico
no tiene DAC, por eso la salida es PWM a unos 488 kHz con un filtro RC que deja solo la señal.

Cada quien escribe además **su sección** en `informe/secciones/` (A → `1_senales.tex`,
B → `2_sistema.tex`, C → `3_emisor.tex`, D → `4_validacion.tex` + intro + integrar).

## Dependencias

```
            parametros.json  (todos lo leen; nadie escribe números a mano)
                   │
                   ▼
   A: senal_del_modo()  ──►  B: sistema completo
          │      │
          │      └────────►  C: compara lo medido a la salida de la Pico
          └───────────────►  D: referencia para validar el receptor
```

A es lo primero que conviene terminar: `senal_del_modo()` es una función corta y
desbloquea a los otros tres. Mientras tanto, B puede avanzar con
`senales.generar_caso('caso_1')` de la Tarea 1, y C y D no la necesitan para empezar.

## Parámetros

- `metadatos/configuraciones.json` (Tarea 1): fs = 16 kHz, N = 1024, Hann, 50 % de
  solapamiento, y los tres casos de ruido. **No se tocan.**
- `taller_semana12/parametros.json` (nuevo): los tres modos del emisor, datos del
  receptor y los márgenes de validación.

| Modo | Señal | Prueba del proyecto |
|---|---|---|
| 1 | Tono de 440 Hz | P1 |
| 2 | 440 + 880 + 1320 Hz (1,0 / 0,55 / 0,3) | P2 |
| 3 | Modo 2 + interferencia de 3500 Hz (SNR = 2 dB) | caso_1 de la Tarea 1 |

Si se cambia un número, se cambia **en el JSON** y en el mismo commit se avisa al grupo.

## Cómo correr

Desde la raíz del repo:

```bash
python taller_semana12/comun/parametros.py              # muestra los parámetros cargados
python taller_semana12/a_senales/prototipo_frecuencias.py
python taller_semana12/b_sistema/sistema_completo.py
python taller_semana12/c_emisor/generar_tabla_seno.py   # escribe tabla_seno.h para el sketch
python taller_semana12/d_validacion/comparar_fft.py
bash   taller_semana12/d_validacion/renderizar.sh       # diagramas .mmd → PNG
```

Cada script imprime sus pasos como `[ok]` o `[PENDIENTE]`. Al inicio todo sale
pendiente, y así cualquiera ve en qué va cada bloque sin preguntar.

Los `TODO (A)`, `TODO (B)`, etc. marcan lo que le toca a cada quien. Las firmas de las
funciones son una propuesta y se pueden cambiar, pero avisando si otro bloque las usa.

## Flujo de Git

```bash
# Una sola vez, quien sube esta estructura:
git checkout dev && git pull
git checkout -b taller-base
git add taller_semana12 && git commit -m "Estructura base del taller semana 12"
git checkout dev && git merge --no-ff taller-base && git push

# Cada persona:
git checkout dev && git pull
git checkout -b taller-senales        # o taller-sistema / taller-emisor / taller-validacion
# ... trabajar con commits pequeños y frecuentes ...
git push -u origin taller-senales
# al terminar: merge --no-ff a dev (o pull request)
```

Reglas del repo (README principal): nadie commitea directo en `dev` ni en `main`. Cada
quien toca **solo su carpeta y su .tex**. Si hace falta cambiar `parametros.json` o
`comun/`, va en un commit aparte con un mensaje claro.

## Checklist de la entrega

- [ ] A (Luis): `senal_del_modo()` lista y `1_senales.tex` escrito
- [ ] B: los tres modos corren de punta a punta y `2_sistema.tex` escrito
- [ ] C: la Pico saca los tres modos, hay captura que lo verifica y `3_emisor.tex` escrito
- [ ] D: cuatro diagramas, márgenes en `parametros.json` y `4_validacion.tex` escrito
- [ ] D: `informe/taller.tex` compila y no queda ningún `[PENDIENTE]` en el PDF
- [ ] Merge de las cuatro ramas a `dev`
- [ ] Zip con `taller.pdf` + carpeta `taller_semana12/` subido a TEC-Digital

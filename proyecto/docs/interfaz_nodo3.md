# Integración e interfaz del Nodo 3

El sketch `firmware/nodos23_esp32/nodos23_esp32.ino` ofrece una interfaz local por
USB, a **115200 baud**, con una orden por línea. Funciona con el monitor serie de
Arduino; no depende de que el script de comandos de Kevin esté implementado.

## Demostración local

Conectar un ESP32 clásico con DAC y cargar el sketch. Enviar:

```text
DEMO
N 1024
METODO ESPECTRAL
UMBRAL 1
ESTADO
```

Esperar varios saltos antes de pedir ESTADO. Se genera en el ESP32 un tono útil
de 500 Hz y una interferencia de 3500 Hz, de amplitud 0,3 cada uno. El diagnóstico
es conocido y se marca explícitamente como DEMO; no se presenta como resultado
del clasificador de la Pico. La salida se dirige al DAC2, GPIO26. DAC1 permanece
centrado durante DEMO. Este modo permite validar el Nodo 3 sin el protocolo ni
el filtro temporal, pero no sustituye las pruebas distribuidas P1–P6.

Para comparar con paso sin reducción, conservando la reconstrucción y su retraso:

```text
METODO PASO
ESTADO
METODO ESPECTRAL
UMBRAL 0.5
ESTADO
```

Cada cambio inicia una nueva medición: no se mezclan resultados de parámetros
anteriores. La demostración usa bandas útiles de 437,5–562,5 Hz y de interferencia
de 3437,5–3562,5 Hz. La referencia y la entrada se retrasan un salto al evaluar
la salida. El relleno inicial no se contabiliza; los transitorios posteriores sí.
La energía se mide con FFT rectangular sobre saltos no solapados y alineados,
con iguales bandas y escalas para las tres señales.

## Órdenes

| Orden | Comportamiento |
|---|---|
| `AYUDA` | Lista las órdenes. |
| `ESTADO` | Imprime parámetros solicitados y aplicados, métricas y contadores. |
| `DEMO` | Activa la fuente sintética local con referencia limpia. |
| `VIVO` | Recibe muestras y diagnóstico de la Pico por UART2. |
| `N 256`, `N 512`, `N 1024`, `N 2048` | Cambia N en DEMO. En VIVO se configura N en la Pico y el ESP32 sigue el largo recibido. |
| `UMBRAL 0..1` | Intensidad del filtro espectral. |
| `ANCHO 0..16000` | Ancho total en Hz; cero usa el diagnóstico. |
| `METODO ESPECTRAL` | Activa la reducción con el umbral configurado. |
| `METODO PASO` | Ganancia espectral unidad, con el mismo retraso. |
| `REINICIAR` | Reinicia historial y mediciones al próximo salto. |

Los comandos numéricos rechazan NaN, infinitos, valores fuera de rango y
argumentos extra. Las líneas tienen un límite de 95 caracteres. Una respuesta
`OK solicitado` indica que el cambio se aplicará al siguiente salto; comparar
`version` con `aplicada` en ESTADO para verificarlo. No se escribe a flash.

## Salida DAC y concurrencia

Hay dos búferes circulares de bytes, uno por DAC. Las tareas convierten la salida
float al rango del DAC y la encolan. El temporizador de 16 MHz solicita una muestra
cada 1000 ticks. Su ISR solo notifica a una tarea prioritaria que llama `dacWrite`
fuera de la interrupción.

Si la tarea se atrasa, omite las muestras vencidas y registra
`ticks_DAC_atrasados`; no las reproduce como una ráfaga acelerada. Los contadores
`vacios` y `llenos` indican falta de muestras y desbordamiento. Antes de recibir
el primer bloque la salida permanece centrada. Esta implementación exige validar
jitter y consumo en la placa; si pierde plazos, corresponde usar el controlador
DAC continuo/DMA con el responsable de la base del ESP32.

La configuración y las instantáneas de estado se protegen con una sección crítica.
El filtro solo se modifica desde su tarea. Cada salto lleva la versión de la
sesión; se descartan los que quedaron encolados antes de un cambio. La cola DAC
rechaza asimismo resultados calculados con una versión anterior. Los mensajes
USB se imprimen fuera de las secciones críticas.

`computo_medio_us`, `computo_max_us` y `carga_computo_pct` cubren el integrador
(filtro y acumulación de métricas/bandas), no transporte, escritura DAC ni toda
la latencia ADC–DAC. `memoria_nodo3_bytes` mide el estado de procesamiento;
las colas, pilas y otros búferes ocupan memoria adicional. Las estadísticas de
calidad y tiempos se reinician por sesión; los contadores de errores son acumulativos.

## Validación

Desde la raíz:

```bash
make -C proyecto/pruebas_c test
arduino-cli compile --libraries proyecto/firmware/libraries -b esp32:esp32:esp32 proyecto/firmware/nodos23_esp32
```

La suite incluye la integración real en C y la interfaz C++ con periféricos
simulados. Las pruebas C cubren los cuatro tamaños, reducción y paso, referencia
ausente y reinicio. El doble Arduino prueba comandos, límites de líneas, cambios
de versión, saturación del DAC y estado. No simula el planificador, la ISR,
el enlace físico ni la concurrencia real, y no valida las APIs del core.
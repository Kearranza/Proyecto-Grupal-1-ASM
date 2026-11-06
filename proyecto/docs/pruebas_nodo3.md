# Pruebas y entrega técnica del Nodo 3

Validación de escritorio.

## Reproducción y evidencia

Desde la raíz del repositorio:

```bash
make -C proyecto/pruebas_c test
python3 proyecto/pruebas_c/validar_nodo3.py
```

Se necesitan Python 3, GNU Make y compiladores C/C++ con soporte de AddressSanitizer
y UndefinedBehaviorSanitizer. No requiere paquetes Python adicionales.
El segundo comando guarda registros en `proyecto/resultados/validacion_nodo3/`:

- `suite.log`: pruebas funcionales, incluyendo la interfaz simulada.
- `*_sanitizadores.log`: pruebas instrumentadas de los cinco ejecutables del Nodo 3.
- `*_compilacion.log`: diagnósticos con advertencias tratadas como errores.
- `reporte.json`: comandos exactos, códigos de retorno, plataforma, fecha UTC y
  hashes SHA-256 de fuentes y pruebas para identificar la versión evaluada.
- `compilador.log`: versión del compilador utilizado.

El programa termina con código no nulo si falla una ejecución. La suite global
permite módulos ajenos marcados PENDIENTE: su éxito no significa que todo el
sistema esté listo. Los registros de sanitizadores del intento inicial conservan
el error de LeakSanitizer bajo ptrace. La ejecución final desactiva únicamente
la detección de fugas (`ASAN_OPTIONS=detect_leaks=0`); mantiene detección de
accesos inválidos y comportamiento indefinido. No se verificaron fugas de memoria.

## Cobertura

| Prueba | Qué verifica | Qué no demuestra |
|---|---|---|
| FFT: 39 comprobaciones | DFT de referencia, señales reales/complejas, inversa, tamaños y argumentos | Tiempo de cómputo en Pico/ESP32 |
| Espectral: 22 comprobaciones | Hann/solapamiento, arranque y vaciado, adaptación, banda, ruido blanco, parámetros e impulsos finitos | Calidad auditiva general o eliminación eficaz de impulsos |
| Métricas: 12 comprobaciones | Referencia existente, fórmulas, casos límite, Parseval, bandas, tiempos y errores | Sincronización física de referencia y ADC |
| Integración: 11 comprobaciones | DEMO, paso, cuatro tamaños, referencia ausente, reinicio y parser | Concurrencia real de FreeRTOS |
| Regresión: 3 comprobaciones | Rechazo sin consumir datos inválidos, referencia intermitente y superposición | Campaña P5 completa |
| Interfaz C++ simulada | Comandos, límites de línea, versiones, cola DAC y estados no disponibles | API real del core, UART, ISR, jitter o DAC |

Las 87 comprobaciones de C propias del Nodo 3 y el ejecutable de interfaz deben
pasar. En el estado actual quedan seis comprobaciones pendientes: una del
protocolo, dos del IIR y tres del diagnóstico. La prueba de identidad cubre N de
8 a 2048; la integración usa exclusivamente los tamaños 256, 512, 1024 y 2048.

## Resultados sintéticos relevantes

- DEMO: tono útil de 500 Hz más interferencia de 3500 Hz, amplitud 0,3 cada uno,
  32 saltos. Para los cuatro N, mejora de SNR aproximada de **31,78 dB** y energía
  útil de **100,00 %**. Se incluye el primer transitorio evaluable; no se cuentan
  las muestras iniciales sin referencia alineada. En PASO, la mejora es 0 dB.
- Ruido blanco con seno: mejora de SNR aproximada de **4,58 dB**, para la semilla
  y condiciones de `test_espectral.c`. No extrapolar a voz o audio arbitrario.
- Superposición: referencia con tonos de 500 y 3500 Hz, más interferencia de
  3500 Hz en fase. Se obtiene **50,116 %** de energía útil por bandas,
  **33,057 dB** de atenuación en la banda interferente y apenas **0,015 dB** de
  mejora de SNR. El filtro también elimina la componente útil de 3500 Hz.

Los umbrales del código son controles de regresión sintética. No se modificaron
para hacer pasar mediciones finales. `config/pruebas.json` mantiene criterios
experimentales sin fijar: el equipo debe acordarlos por condición antes de medir,
particularmente cuando señal y ruido se superponen. No tratar los resultados
anteriores como tres repeticiones independientes ni inventar variabilidad cero.

## Documentación

- `espectral.md`: algoritmo, parámetros, reconstrucción y limitaciones.
- `metricas.md`: fórmulas, bandas, alineación y significado de NaN/inf.
- `interfaz_nodo3.md`: comandos, flujo de datos, salida y concurrencia.
- `paper/secciones/2_teoria.tex`: fundamentos con referencias primarias.
- `paper/secciones/5_espectral.tex`: implementación y alcance de la validación.


# Protocolo entre placas (v0)

Responsable: Kevin. Lo implementan `firmware/libraries/asm_comun/src/trama.c` (las dos
placas) y `pc/trama.py` (la computadora). Los números salen de `config/sistema.json`.
Si algo cambia aquí, cambia en los tres lugares en el mismo commit.

## Enlace

| Parámetro | Valor | Por qué |
|---|---|---|
| Medio | UART, Pico W GP8 (TX) → ESP32 GPIO16 (RX), tierra común | Alcanza la velocidad, es simple en las dos placas; SPI obligaría a programar el ESP32 como esclavo |
| Velocidad | 921 600 baud, 8N1 | 16 000 muestras/s × 2 B = 32 kB/s = 320 kbit/s en 8N1; da 2,9× de margen. Error de reloj < 0,1 % en ambas placas |
| Formato de muestra | int16 little-endian, centrada en 0 | El ADC es de 12 bits; float duplicaría el tráfico. Ambas placas son little-endian |

## Trama

```
| 0xAA | 0x55 | tipo | seq (u16) | largo (u16) | carga (largo bytes) | crc (u16) |
   1      1      1        2            2              0..4096              2
```

- Enteros en **little-endian**.
- `seq` aumenta en 1 en cada trama enviada y da la vuelta en 65535 → 0. El receptor
  suma `seq - seq_previo - 1` a `bloques_perdidos` (requisito 5.3).
- `crc`: **CRC-16/CCITT-FALSE** (polinomio 0x1021, inicial 0xFFFF, sin reflejar, sin
  XOR final) calculado sobre `tipo`, `seq`, `largo` y `carga`. Vector de prueba:
  `"123456789"` → `0x29B1` (requisito 5.2).
- Si el CRC falla, el receptor descarta la trama, suma `errores_crc` y vuelve a buscar
  `0xAA 0x55` desde el byte siguiente.
- Sobrecarga: 9 bytes por trama, 0,4 % con un salto de 1024 muestras.

## Tipos

| Tipo | Valor | Carga | Quién la manda |
|---|---|---|---|
| MUESTRAS | 1 | `int16[n]` con las N/2 muestras nuevas del salto | Nodo 1, cada salto |
| DIAGNOSTICO | 2 | `diagnostico_t` (16 bytes, ver `diagnostico.h`) | Nodo 1, cada bloque |
| COMANDO | 3 | texto ASCII, p. ej. `N 512` | Reservado (de vuelta al nodo 1 por GPIO17 → GP9) |
| ESTADO | 4 | contadores: desbordamientos, saltos procesados | Nodo 1, cuando cambian |

`diagnostico_t`: `uint8 tipo, uint8 reservado, uint16 n_bloque, float frecuencia_hz,
float ancho_hz, float nivel_db`. En Python: `struct.Struct('<BBHfff')`.

## Decidido

**Cada cuánto se manda ESTADO.** Como máximo una vez por segundo, y solo si algún
contador cambió desde el último envío. Son 12 bytes de carga más 9 de trama: unos
21 B/s contra los 32 kB/s del flujo de muestras, o sea 0,07 %. Mandarlo por bloque
(31,25 veces por segundo) no aportaría nada, porque los contadores casi nunca
cambian, y mandarlo solo al final dejaría sin evidencia una prueba que se corte.

**Qué pasa si N cambia a mitad de un bloque.** El receptor deduce N del campo
`largo` de la trama MUESTRAS: con 50 % de solapamiento, `largo = N` bytes
(N/2 muestras de 2 bytes). Cuando llega un `largo` distinto al que venía usando:

1. descarta lo que tenga acumulado del bloque en curso;
2. vuelve a inicializar los módulos con el nuevo N (búferes de solapamiento,
   ventana y tablas de la FFT);
3. marca el primer bloque completo del nuevo N como *de arranque*.

Ese primer bloque sale atenuado en sus bordes, porque el solapamiento todavía no
tiene historia con la que sumar, así que **no se usa para medir**. El nodo 1 avisa
del cambio mandando un ESTADO justo después, para que quede registrado en qué
número de bloque ocurrió.

## Herramientas

- `pc/trama.py` implementa lo mismo en Python y trae su autoprueba
  (`python proyecto/pc/trama.py`). Uno de sus pasos compila
  `pruebas_c/volcar_trama.c` y compara byte a byte la trama que arma C con la que
  arma Python: si alguien cambia una de las dos implementaciones y no la otra, esa
  prueba falla.
- `pruebas_c/test_trama.c` cubre el contrato en C: vector del CRC, formato,
  rechazo de carga grande, ida y vuelta, resincronización, detección de un bit
  cambiado y conteo de bloques perdidos.

## Sonda del enlace

`firmware/sonda_pico/` convierte una Pico comun en el adaptador USB-TTL que necesita
`pc/registrar.py`. Escucha en GP9 la misma linea que el nodo 1 manda al ESP32 por GP8 y
vuelca los bytes al USB sin transmitir nada, asi que el ESP32 sigue recibiendo igual y no
hay que tocar su firmware.

La sonda no imprime texto por el USB a proposito. El lector de `trama.py` resincroniza con
`AA 55` y descarta lo que no cuadre, asi que texto *entre* tramas no estorba (40 de 40
recuperadas en la prueba), pero texto que parte una trama por la mitad las pierde todas
(0 de 40, 40 errores de CRC). El estado de la sonda va por el LED: fijo si esta llegando
flujo, parpadeo lento si no llega nada.

## Emisora de senal arbitraria (P3)

P3 pide una senal que no sea multitono. En vez de meterla por la tarjeta de sonido,
`firmware/emisor_tabla/` la reproduce desde una tabla en flash por el mismo PWM de GP15 y
el mismo RC que P1 y P2. `pc/tabla_senal.py` genera la tabla desde `senales/p3.wav`, que
sigue siendo la referencia con la que `campana.py` calcula las metricas.

Son 32 768 muestras de 8 bits, 32 kB de los 2 MB de flash de la Pico. El error contra el
WAV es de medio nivel de 255, solo el redondeo a 8 bits. Asi la senal de P3 es identica en
las tres repeticiones, cosa que reproduciendo desde la computadora no se puede garantizar,
y no hacen falta ni capacitor de acople ni divisor ni cable de audio.

Comandos por USB: `TOCAR`, `PARAR`, `LAZO ON|OFF`, `ESTADO`.

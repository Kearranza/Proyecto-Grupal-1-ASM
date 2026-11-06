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


## Sonda y emisora en la misma Pico

Con tres placas no alcanza para tener emisora y sonda por separado: la Pico W es el nodo 1
y el ESP32 son los nodos 2 y 3, asi que la Pico que queda hace las dos cosas a la vez.
`firmware/sonda_emisor/` emite por PWM en GP15 y escucha el enlace en GP9. Son perifericos
distintos y no se estorban: la interrupcion de muestreo solo hace una lectura de tabla y
una escritura de PWM, y como interrumpe al lazo principal, reenviar bytes no le altera el
tiempo.

    PWM GP15  -> RC de 2 etapas -> Pico W GPIO26    emite la senal de prueba
    UART GP9  <- Pico W GP8                          escucha el enlace, en paralelo con el ESP32
    USB       -> pc/registrar.py                     entrega las tramas

El UART1 TX se manda a GP4, un pin sin conectar, para no pelear con el GP8 del nodo 1,
que es una salida.

**El texto no puede partir una trama.** El lector de `trama.py` resincroniza con `AA 55`,
asi que texto *entre* tramas no estorba (40 de 40 recuperadas al medirlo) pero texto que
parte una trama las pierde todas (0 de 40, 40 errores de CRC). Por eso el sketch sigue la
estructura de la trama —sync, tipo, seq, largo, carga, CRC— y guarda las respuestas de los
comandos hasta que no hay ninguna a medias. Probado contra tramas reales: sobre 4635 bytes
marca los 3 limites exactos y ninguno de mas, incluso con una carga que empieza en `0xAA`.

Comandos por USB: `TOCAR`, `PARAR`, `LAZO ON|OFF`, `ESTADO`. El LED queda fijo si esta
llegando flujo y parpadea lento si no llega nada.


## La ruta acustica y la referencia

La senal va de la emisora al microfono por el aire: PWM en GP15 -> RC -> amplificador ->
parlante -> aire -> microfono -> GPIO26 de la Pico W.

Eso obliga a cambiar de donde sale la referencia de las metricas. El parlante y el cuarto
colorean lo que llega, asi que comparar contra el WAV original mediria tambien la sala.
Se graban dos pasadas por el mismo camino, sin mover nada entre una y otra:

1. `TOCAR LIMPIA` y se registra en `{id}_ref`: esa captura es la referencia.
2. `TOCAR CONTAMINADA` y se registran `{id}_r1`, `{id}_r2` y `{id}_r3`.

Lo que el cuarto agrega esta en las dos por igual y se cancela al comparar. Si se mueve el
microfono, el parlante o el volumen, hay que repetir la referencia.

`campana.py` usa la captura de referencia cuando existe y lo anota en la columna `ruta`
del CSV. Si falta, avisa en vez de medir contra el WAV y dar un numero que parece bueno
pero incluye la sala. Las dos tablas, limpia y contaminada, viven juntas en el flash y se
eligen por comando: 4 segundos de cada una son 125 kB de los 2048 kB disponibles.
`pc/tabla_senal.py p3|p4|p5` las genera desde los WAV de `senales/`.

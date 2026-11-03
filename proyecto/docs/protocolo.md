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

## Pendiente de decidir (Kevin)

- ¿Cada cuánto se manda ESTADO?
- Qué hace el ESP32 si cambia N a mitad de un bloque (sugerencia: reiniciar el
  procesamiento al recibir el primer salto con el nuevo largo).

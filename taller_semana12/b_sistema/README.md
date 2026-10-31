# Bloque B: sistema en alto nivel

Desde la raiz del repositorio, con Python 3, NumPy, SciPy y Matplotlib:

```bash
python3 taller_semana12/b_sistema/sistema_completo.py
python3 -m unittest discover -s taller_semana12/b_sistema -p 'test_*.py' -v
```

La ejecucion falla con codigo distinto de cero si una etapa falla. Los resultados
se guardan exclusivamente en `taller_semana12/resultados/` con prefijo `b_`.

## Conexion A-B-D

```bash
python3 taller_semana12/d_validacion/comparar_fft.py taller_semana12/resultados/b_picos_modo_2.csv --modo 2
```

Ese comando valida picos; un CSV de picos no contiene las muestras necesarias
para calcular SNR. La comprobacion de SNR se hace en B con la captura temporal.
Los resultados son simulados: no validan CRC, transporte, DMA ni tiempos reales.
Los umbrales de deteccion de interferencia de B son distintos de los margenes de
aceptacion de D: uno selecciona filtros, el otro valida la captura del emisor.

## Conexion pendiente con C

- TODO (C): confirmar frecuencia de corte, ganancia y topologia del RC. `fc_hz=null`
  omite el RC. La propuesta de 10.6 kHz en D no se activa automaticamente: excede
  Nyquist del modelo discreto actual y requiere revisar el modelo analogico o
  usar sobremuestreo. Tampoco se modela la carga mutua de etapas pasivas.
- La entrega conjunta debe incluir `src/` y `metadatos/configuraciones.json`.

# Bloque B: sistema en alto nivel

Desde la raiz del repositorio, con Python 3, NumPy, SciPy y Matplotlib:

```bash
python3 taller_semana12/b_sistema/sistema_completo.py
python3 -m unittest discover -s taller_semana12/b_sistema -p 'test_*.py' -v
```

La ejecucion falla con codigo distinto de cero si una etapa falla. Los resultados
se guardan exclusivamente en `taller_semana12/resultados/` con prefijo `b_`.

## Interfaces pendientes

- TODO (A): conectar `entrada` con `senal_del_modo` sin cuantizacion, acordando
  fases, escala y numero de muestras. El sustituto local usa fases cero y escala
  comun para la referencia y mezcla. No afirma equivalencia muestra a muestra
  con la Tarea 1 ni con el futuro emisor.
- TODO (C): confirmar frecuencia de corte, ganancia y topologia del RC. `fc_hz=null`
  omite el RC. Si se proporciona fc, se modelan dos polos aislados con respuesta
  exponencial, no la carga mutua de una red pasiva ni el rizado de la portadora.
- TODO (D): acordar formato del diagnostico y margenes externos. Los umbrales de
  `configuracion_b.json` son decisiones y pruebas locales de B. D debe integrar
  `2_sistema.tex` y empaquetar tambien `src/` y `metadatos/configuraciones.json`.

// Constantes compartidas y codigos de retorno de todos los modulos.
// Los numeros vienen de config/sistema.json (ver config_generado.h).
#pragma once
#include "config_generado.h"

#define SALTO_MAX (N_MAX / 2)   // muestras nuevas por bloque con 50 % de solapamiento

// Todos los modulos devuelven uno de estos codigos.
// ASM_PENDIENTE marca una funcion que su responsable todavia no implementa:
// las pruebas la reportan como [PENDIENTE] en vez de [FALLA].
#define ASM_OK          0
#define ASM_ERROR      -1
#define ASM_PENDIENTE -99

static inline int n_es_valido(int n) {
    return n >= 8 && n <= N_MAX && (n & (n - 1)) == 0;
}

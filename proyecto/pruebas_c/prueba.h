// Mini marco de pruebas para los modulos de asm_comun, compilados con gcc en la PC.
// Cada prueba termina en [ok], [PENDIENTE] (el modulo devuelve ASM_PENDIENTE o
// faltan datos de referencia) o [FALLA]. Solo las FALLAS hacen fallar `make test`.
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "asm_comun.h"

static int prueba_fallas = 0, prueba_pendientes = 0, prueba_ok = 0;

#define OK(nombre) do { prueba_ok++; printf("  [ok]        %s\n", nombre); } while (0)
#define PENDIENTE(nombre, motivo) do { prueba_pendientes++; \
    printf("  [PENDIENTE] %s: %s\n", nombre, motivo); } while (0)
#define FALLA(nombre, ...) do { prueba_fallas++; printf("  [FALLA]     %s: ", nombre); \
    printf(__VA_ARGS__); printf("\n"); } while (0)

// Devuelve 1 si el codigo indica que el modulo no esta hecho (y lo reporta)
static inline int es_pendiente(int codigo, const char *nombre, const char *responsable) {
    if (codigo == ASM_PENDIENTE) {
        char motivo[96];
        snprintf(motivo, sizeof motivo, "sin implementar (%s)", responsable);
        PENDIENTE(nombre, motivo);
        return 1;
    }
    return 0;
}

static inline int resumen(const char *modulo) {
    printf("  -> %s: %d ok, %d pendientes, %d fallas\n", modulo,
           prueba_ok, prueba_pendientes, prueba_fallas);
    return prueba_fallas ? 1 : 0;
}

// Abre un archivo de referencia generado por pc/referencias_c.py
static inline FILE *abrir_referencia(const char *ruta, const char *nombre) {
    FILE *f = fopen(ruta, "r");
    if (!f) PENDIENTE(nombre, "faltan datos: correr python proyecto/pc/referencias_c.py");
    return f;
}

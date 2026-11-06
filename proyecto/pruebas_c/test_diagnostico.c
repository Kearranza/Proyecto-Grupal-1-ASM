// Contrato de diagnostico.c (Luis): con el espectro de un bloque de cada caso de la
// Tarea 1, identifica el tipo de ruido y la frecuencia de la interferencia.
// Ojo: lo esperado es la verdad del caso. clasificar_ruido() de la Tarea 1 falla en
// caso_3 (dice tonal) y en caso_1 toma el tono util de 440 Hz como interferencia.
#include "prueba.h"

static void probar_caso(const char *caso) {
    char ruta[64], nombre[80];
    snprintf(ruta, sizeof ruta, "datos/diagnostico_%s.txt", caso);
    snprintf(nombre, sizeof nombre, "diagnostico de %s", caso);
    FILE *f = abrir_referencia(ruta, nombre);
    if (!f) return;
    int n, tipo;
    float fs, frecuencia;
    static float magnitud[N_MAX / 2 + 1];
    int valido = fscanf(f, "%d %f %d %f", &n, &fs, &tipo, &frecuencia) == 4 && n <= N_MAX;
    for (int i = 0; valido && i <= n / 2; i++) valido = fscanf(f, "%f", &magnitud[i]) == 1;
    fclose(f);
    if (!valido) { FALLA(nombre, "archivo invalido"); return; }

    diagnostico_t d;
    int r = diagnostico_estimar(magnitud, n, fs, &d);
    if (es_pendiente(r, nombre, "Luis")) return;
    float tolerancia = tipo == RUIDO_BANDA ? 150.0f : fs / n;   // banda: centro aproximado
    if (d.tipo != tipo) FALLA(nombre, "tipo %d, se esperaba %d", d.tipo, tipo);
    else if (tipo != RUIDO_BLANCO && fabsf(d.frecuencia_hz - frecuencia) > tolerancia)
        FALLA(nombre, "frecuencia %.1f Hz, se esperaba %.1f Hz", d.frecuencia_hz, frecuencia);
    else OK(nombre);
}

int main(void) {
    printf("\n== diagnostico.c ==\n");
    probar_caso("caso_1");
    probar_caso("caso_2");
    probar_caso("caso_3");
    return resumen("diagnostico.c");
}

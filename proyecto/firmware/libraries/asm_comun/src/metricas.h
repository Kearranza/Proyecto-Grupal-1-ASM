// Metricas obligatorias (Gretchell), seccion 3.5 del proyecto.
// Se acumulan por bloques para no guardar la senal entera en el micro.
// La referencia de validacion es src/metricas.py de la Tarea 1.
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double suma_ref2;      // sum s^2
    double suma_err_ent2;  // sum (s - x)^2, entrada sin filtrar
    double suma_err_sal2;  // sum (s - y)^2, salida filtrada
    long muestras;
} metricas_t;

typedef struct {
    float mse;
    float snr_entrada_db;
    float snr_salida_db;
    float delta_snr_db;
    // TODO (Gretchell): energia util conservada (%) y atenuacion de la
    // interferencia principal (dB) necesitan energia por bandas; latencia por
    // bloque se mide con el reloj del micro. Agregar los campos y su calculo.
} resultado_metricas_t;

void metricas_reiniciar(metricas_t *m);
// s: referencia limpia, x: contaminada, y: filtrada (alineadas)
int metricas_acumular(metricas_t *m, const float *s, const float *x, const float *y, int n);
int metricas_calcular(const metricas_t *m, resultado_metricas_t *r);

#ifdef __cplusplus
}
#endif

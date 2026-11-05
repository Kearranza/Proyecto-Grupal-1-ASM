// Usar una instancia por metodo y reiniciarla entre condiciones experimentales.
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        double suma_ref2;
        double suma_err_ent2;
        double suma_err_sal2;
        long muestras;
        double energia_util_ref;
        double energia_util_sal;
        double energia_interferencia_ent;
        double energia_interferencia_sal;
        long bloques_bandas;
        double tiempo_total_us;
        double periodo_total_us;
        uint32_t tiempo_max_us;
        long bloques_tiempo;
    } metricas_t;

    typedef struct
    {
        float mse;
        float snr_entrada_db;
        float snr_salida_db;
        float delta_snr_db;
        float energia_util_pct;
        float atenuacion_interferencia_db;
        float tiempo_medio_us; // computo medido,no incluye transporte ni DAC
        float tiempo_max_us;
        float carga_pct; // 100 * tiempo de computo / periodo disponible
    } resultado_metricas_t;

    // Acepta NULL como operacion vacia.
    void metricas_reiniciar(metricas_t *m);
    // s: referencia limpia, x: contaminada, y: filtrada, alineadas y de igual escala.
    // n > 0. Un salto invalido no modifica el acumulador.
    int metricas_acumular(metricas_t *m, const float *s, const float *x, const float *y, int n);
    // Energias no negativas, medidas sobre iguales intervalos, ventanas y bandas.
    // La banda util requiere una referencia limpia, el ruido que coincida con ella
    // puede sobreestimar la energia conservada. No se recorta el porcentaje a 100.
    int metricas_acumular_bandas(metricas_t *m, double util_ref, double util_sal,
                                 double interferencia_ent, double interferencia_sal);
    // FFT completa sin escala directa, de una senal real
    // Devuelve sum(|X[k]|^2)/N en la banda, con las parejas conjugadas contadas.
    // Si hubo ventana, la energia corresponde a la senal ventaneada, sin compensarla.
    int metricas_energia_banda(const float *re, const float *im, int n, float fs,
                               float desde_hz, float hasta_hz, double *energia);
    // tiempo_us: duracion medida externamente, periodo_us: plazo entre saltos (>0).
    int metricas_acumular_tiempo(metricas_t *m, uint32_t tiempo_us, uint32_t periodo_us);
    // Requiere muestras temporales. Sin bandas o tiempos sus campos devuelven NaN.
    // Cocientes positivos/0 producen +inf; 0/positivo, -inf en dB; 0/0, NaN.
    // Con referencia de energia cero, delta SNR tambien es NaN.
    int metricas_calcular(const metricas_t *m, resultado_metricas_t *r);

#ifdef __cplusplus
}
#endif

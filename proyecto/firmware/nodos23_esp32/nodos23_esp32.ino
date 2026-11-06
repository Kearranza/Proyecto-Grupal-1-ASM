// Nodos 2 y 3 en ESP32 clasico. La adquisicion permanece en la Pico W.
// Interfaz USB a 115200: AYUDA muestra las ordenes disponibles.
#include <Arduino.h>
#include <asm_comun.h>
#include <nodo3.h>
#include "driver/dac_continuous.h"

struct configuracion_t
{
    uint32_t version;
    int n;
    float umbral, ancho;
    bool demo, paso;
};
struct salto_t
{
    uint32_t id, epoca, version;
    uint16_t n;
    diagnostico_t diagnostico;
    float x[SALTO_MAX];
};
struct estado_t
{
    uint32_t saltos, errores, reinicios, sin_diagnostico;
    int n, codigo_temporal;
    uint32_t version_aplicada;
    bool demo, paso, tiene_referencia;
    resultado_metricas_t resultados;
    uint32_t tiempo_us, max_us;
    double suma_us;
    long muestras;
};
static configuracion_t configuracion = {1, N_POR_DEFECTO, 1.0f, 0.0f, false, false};
static estado_t estado;
static portMUX_TYPE candado = portMUX_INITIALIZER_UNLOCKED;
static lector_trama_t lector;
static trama_t trama;
static uint32_t tramas_ok, errores_crc, perdidas, descartes, descartes_temporal;
static bool protocolo_pendiente;
static QueueHandle_t cola_temporal, cola_espectral;
static dac_continuous_handle_t dac;
static uint32_t dac_frecuencia, dac_bytes_por_s;
static volatile bool listo;

// Prototipo explicito: evita que Arduino lo adelante a la definicion del tipo.
static configuracion_t leer_configuracion();

static configuracion_t leer_configuracion()
{
    portENTER_CRITICAL(&candado);
    configuracion_t c = configuracion;
    portEXIT_CRITICAL(&candado);
    return c;
}

// Los dos DAC salen por DMA (dac_continuous, canales alternados: DAC1, DAC2, DAC1...).
// Antes una interrupcion despertaba una tarea 16 000 veces por segundo para hacer
// dacWrite: eso se comia el nucleo 0, la recepcion no alcanzaba a vaciar la UART y
// se perdian tramas (574 de 664 con error de CRC) hasta disparar el watchdog.
static const unsigned CAPACIDAD_DAC = 2 * N_MAX;
struct salida_t
{
    uint8_t datos[CAPACIDAD_DAC];
    unsigned lectura, cantidad, salto;
    bool activa, cebando;   // cebando: espera un salto completo antes de sonar
    uint32_t vacios, llenos, recortes;   // vacios: veces que se quedo sin muestras
};
static salida_t salidas[2];
static uint32_t temporal_max_us, temporal_redisenos;   // diagnostico del nodo 2
static int temporal_tipo = -1;
static uint32_t ticks_atrasados;   // trozos que el DMA no acepto a tiempo
static const unsigned TROZO_DAC = 128;   // muestras por canal en cada escritura: 8 ms

// Grabacion de lo que sale por los dos DAC, para la demo con un solo amplificador: con
// el ampli en la emisora suena la senal con ruido y aqui quedan los ultimos segundos de
// cada metodo; al pasar el ampli al DAC, REPETIR los toca desde memoria. Repetir desde
// memoria, y no en vivo, evita el lazo parlante -> microfono -> DAC (requisito 5.7).
// En bloques de 4 KB: la RAM libre del ESP32 esta repartida en regiones y no da 32 KB
// seguidos.
static const unsigned BLOQUE_GRAB = 4000, BLOQUES_GRAB = 4 * FS_HZ / BLOQUE_GRAB;   // 4 s
static uint8_t *grabacion[2][BLOQUES_GRAB];
static unsigned grab_n, grab_pos, rep_pos;
static inline uint8_t &grabada(int canal, unsigned i)
{
    return grabacion[canal][i / BLOQUE_GRAB][i % BLOQUE_GRAB];
}
static volatile int repitiendo = -1;   // -1 en vivo, 0 temporal, 1 espectral, 2 tono de prueba

static void vaciar_salida(int canal)
{
    portENTER_CRITICAL(&candado);
    salidas[canal].lectura = salidas[canal].cantidad = 0;
    salidas[canal].activa = false;
    portEXIT_CRITICAL(&candado);
}

static void encolar_salida(int canal, const float *x, unsigned n, uint32_t version)
{
    uint8_t convertido[SALTO_MAX];
    for (unsigned i = 0; i < n; i++)
    {
        float valor = isfinite(x[i]) ? fmaxf(-1.0f, fminf(127.0f / 128.0f, x[i])) : 0.0f;
        convertido[i] = (uint8_t)((int)lroundf(valor * 128.0f) + 128);
    }
    portENTER_CRITICAL(&candado);
    if (configuracion.version != version)
    {
        portEXIT_CRITICAL(&candado);
        return;
    }
    salida_t &s = salidas[canal];
    if (s.cantidad + n > CAPACIDAD_DAC)
    {
        s.llenos++;
        s.cantidad = 0;
        s.lectura = 0;
        s.activa = false;
    }
    unsigned escritura = (s.lectura + s.cantidad) % CAPACIDAD_DAC;
    unsigned primero = n < CAPACIDAD_DAC - escritura ? n : CAPACIDAD_DAC - escritura;
    memcpy(s.datos + escritura, convertido, primero);
    memcpy(s.datos, convertido + primero, n - primero);
    s.cantidad += n;
    s.salto = n;
    if (!s.activa)
        s.cebando = true;
    // La cola oscila entre uno y dos saltos; mas de tres es atraso acumulado (arranque,
    // una rafaga) que el DAC nunca recupera porque consume al mismo ritmo: se descarta
    // lo mas viejo.
    if (s.cantidad > 3 * n)
    {
        unsigned sobra = s.cantidad - 3 * n;
        s.lectura = (s.lectura + sobra) % CAPACIDAD_DAC;
        s.cantidad -= sobra;
        s.recortes++;
    }
    s.activa = true;
    portEXIT_CRITICAL(&candado);
}

// Crea el DAC continuo y mide a que ritmo consume: segun la version del driver, en modo
// alternado la frecuencia es por canal o total. Se prueba FS_HZ y, si cada canal sale a
// la mitad, 2*FS_HZ. APLL porque el reloj por defecto no baja de 19,6 kHz.
static bool iniciar_dac()
{
    static uint8_t silencio[2 * TROZO_DAC];
    memset(silencio, 128, sizeof silencio);
    const uint32_t opciones[2] = {FS_HZ, 2 * FS_HZ};
    for (uint32_t f : opciones)
    {
        dac_continuous_config_t cfg = {DAC_CHANNEL_MASK_ALL, 4, 1024, f, 0,
                                       DAC_DIGI_CLK_SRC_APLL, DAC_CHANNEL_MODE_ALTER};
        if (dac_continuous_new_channels(&cfg, &dac) != ESP_OK)
            continue;
        dac_continuous_enable(dac);
        for (int i = 0; i < 24; i++)   // llenar los 4 KB del DMA antes de medir
            dac_continuous_write(dac, silencio, sizeof silencio, nullptr, -1);
        uint32_t inicio = micros(), bytes = 0;
        while (bytes < FS_HZ)   // medio segundo con los dos canales
        {
            size_t cargados = 0;
            dac_continuous_write(dac, silencio, sizeof silencio, &cargados, -1);
            bytes += cargados;
        }
        dac_frecuencia = f;
        dac_bytes_por_s = (uint32_t)(bytes * 1e6 / (micros() - inicio));
        if (dac_bytes_por_s > 1.8f * FS_HZ && dac_bytes_por_s < 2.2f * FS_HZ)
            return true;   // 2 canales x FS_HZ
        dac_continuous_disable(dac);
        dac_continuous_del_channels(dac);
    }
    return false;
}

// Arma trozos intercalados (temporal, espectral) y se bloquea en el DMA: el ritmo lo
// pone el hardware, asi que la tarea casi no usa CPU.
static void tarea_salida(void *)
{
    while (!listo)
        vTaskDelay(1);
    static uint8_t trozo[2 * TROZO_DAC];
    for (;;)
    {
        portENTER_CRITICAL(&candado);
        for (unsigned i = 0; i < TROZO_DAC; i++)
        {
            for (int canal = 0; canal < 2; canal++)
            {
                salida_t &s = salidas[canal];
                uint8_t valor = 128;
                if (s.activa && s.cebando && s.cantidad >= s.salto)
                    s.cebando = false;
                if (s.activa && !s.cebando)
                {
                    if (s.cantidad)
                    {
                        valor = s.datos[s.lectura];
                        s.lectura = (s.lectura + 1) % CAPACIDAD_DAC;
                        s.cantidad--;
                    }
                    else
                    {
                        // Sin colchon un salto atrasado deja huecos: se vuelve a cebar
                        s.vacios++;
                        s.cebando = true;
                    }
                }
                trozo[2 * i + canal] = valor;
            }
        }
        portEXIT_CRITICAL(&candado);
        int canal = repitiendo;
        if (canal < 0)
        {
            for (unsigned i = 0; i < TROZO_DAC && grab_n; i++)
            {
                grabada(0, grab_pos) = trozo[2 * i];
                grabada(1, grab_pos) = trozo[2 * i + 1];
                grab_pos = (grab_pos + 1) % grab_n;
            }
        }
        else if (canal == 2)
        {
            // Tono de 500 Hz a 80 % de escala: prueba la cadena DAC -> filtro -> ampli
            static uint32_t fase;
            for (unsigned i = 0; i < TROZO_DAC; i++, fase = (fase + 1) % FS_HZ)
                trozo[2 * i] = trozo[2 * i + 1] =
                    (uint8_t)(128 + lroundf(100.0f * sinf(6.2831853f * 500.0f * fase / FS_HZ)));
        }
        else
        {
            // El metodo elegido sale por los dos DAC: suena donde este el filtro
            for (unsigned i = 0; i < TROZO_DAC; i++)
            {
                trozo[2 * i] = trozo[2 * i + 1] = grabada(canal, (grab_pos + rep_pos) % grab_n);
                rep_pos = (rep_pos + 1) % grab_n;
            }
        }
        size_t cargados = 0;
        dac_continuous_write(dac, trozo, sizeof trozo, &cargados, 100);
        if (cargados < sizeof trozo)
        {
            portENTER_CRITICAL(&candado);
            ticks_atrasados++;
            portEXIT_CRITICAL(&candado);
        }
    }
}

static void tarea_recepcion(void *)
{
    while (!listo)
        vTaskDelay(1);
    static salto_t s;
    diagnostico_t d = {RUIDO_NINGUNO, 0, 0, 0, 0, 0};
    uint32_t id = 0, epoca = 0, crc_previo = 0, perdidas_previas = 0;
    for (;;)
    {
        // Acotar el lote permite atender USB incluso con entrada continua.
        for (int bytes = 0; bytes < 4096 && Serial2.available(); bytes++)
        {
            int r = trama_leer_byte(&lector, (uint8_t)Serial2.read(), &trama);
            portENTER_CRITICAL(&candado);
            protocolo_pendiente = r == ASM_PENDIENTE;
            tramas_ok = lector.tramas_ok;
            errores_crc = lector.errores_crc;
            perdidas = lector.bloques_perdidos;
            portEXIT_CRITICAL(&candado);
            if (lector.errores_crc != crc_previo || lector.bloques_perdidos != perdidas_previas)
            {
                epoca++;
                d.n_bloque = 0;
                crc_previo = lector.errores_crc;
                perdidas_previas = lector.bloques_perdidos;
            }
            if (r != 1)
                continue;
            if (trama.tipo == TRAMA_TIPO_DIAGNOSTICO && trama.largo == sizeof(d))
            {
                memcpy(&d, trama.carga, sizeof d);
            }
            else if (trama.tipo == TRAMA_TIPO_MUESTRAS)
            {
                s.id = ++id;
                s.epoca = epoca;
                s.n = trama.largo / 2;
                if (trama.largo % 2 || (s.n != 128 && s.n != 256 && s.n != 512 && s.n != 1024))
                {
                    epoca++;
                    portENTER_CRITICAL(&candado);
                    descartes++;
                    portEXIT_CRITICAL(&candado);
                    continue;
                }
                s.diagnostico = d;
                for (int i = 0; i < s.n; i++)
                {
                    uint16_t bruto = (uint16_t)trama.carga[2 * i] | ((uint16_t)trama.carga[2 * i + 1] << 8);
                    int valor = bruto < 32768 ? bruto : (int)bruto - 65536;
                    s.x[i] = valor / (float)(1 << (ADC_BITS - 1));
                }
                configuracion_t c = leer_configuracion();
                if (!c.demo)
                {
                    // La version viaja con el salto para descartar colas de una sesion anterior.
                    s.version = c.version;
                    if (xQueueSend(cola_temporal, &s, 0) != pdTRUE)
                    {
                        portENTER_CRITICAL(&candado);
                        descartes_temporal++;
                        portEXIT_CRITICAL(&candado);
                    }
                    if (xQueueSend(cola_espectral, &s, 0) != pdTRUE)
                    {
                        portENTER_CRITICAL(&candado);
                        descartes++;
                        portEXIT_CRITICAL(&candado);
                    }
                }
            }
        }
        vTaskDelay(1);
    }
}

static void tarea_temporal(void *)
{
    while (!listo)
        vTaskDelay(1);
    static iir_t filtro;
    static salto_t s;
    static float y[SALTO_MAX];
    diagnostico_t anterior = {};
    bool valido = false;
    uint32_t id = 0, epoca = 0;
    for (;;)
    {
        if (xQueueReceive(cola_temporal, &s, portMAX_DELAY) != pdTRUE)
            continue;
        configuracion_t c = leer_configuracion();
        if (c.demo || s.version != c.version)
        {
            valido = false;
            vaciar_salida(0);
            continue;
        }
        if (s.id != id + 1 || s.epoca != epoca)
        {
            valido = false;
            vaciar_salida(0);
        }
        id = s.id;
        epoca = s.epoca;
        int r = ASM_ERROR;
        uint32_t inicio = micros();
        bool redisenado = false;
        if (s.diagnostico.n_bloque == 2 * s.n)
        {
            if (!valido || memcmp(&anterior, &s.diagnostico, sizeof anterior))
            {
                redisenado = true;
                r = iir_disenar(&filtro, &s.diagnostico, FS_HZ);
                valido = r == ASM_OK;
                if (valido)
                {
                    iir_reiniciar_estado(&filtro);
                    anterior = s.diagnostico;
                }
            }
            if (valido)
                r = iir_procesar(&filtro, s.x, y, s.n);
        }
        uint32_t tiempo = micros() - inicio;
        portENTER_CRITICAL(&candado);
        estado.codigo_temporal = r;
        if (tiempo > temporal_max_us)
            temporal_max_us = tiempo;
        temporal_redisenos += redisenado;
        temporal_tipo = s.diagnostico.tipo;
        portEXIT_CRITICAL(&candado);
        if (r == ASM_OK)
            encolar_salida(0, y, s.n, s.version);
        else
        {
            valido = false;
            vaciar_salida(0);
        }
    }
}

static void tarea_espectral(void *)
{
    while (!listo)
        vTaskDelay(1);
    static nodo3_t p;
    static salto_t s;
    static float y[SALTO_MAX], referencia[SALTO_MAX];
    const bandas_nodo3_t bandas = {1, 437.5f, 562.5f, 3437.5f, 3562.5f};
    uint32_t version = 0, id = 0, epoca = 0, muestra_demo = 0;
    int n = 0;
    TickType_t proxima = xTaskGetTickCount();
    for (;;)
    {
        configuracion_t c = leer_configuracion();
        bool reiniciar = c.version != version;
        if (c.demo)
        {
            if (reiniciar)
            {
                muestra_demo = 0;
                proxima = xTaskGetTickCount();
            }
            s.n = c.n / 2;
            s.diagnostico = {RUIDO_TONAL, 0, (uint16_t)c.n, 3500.0f, 0, 0};
            nodo3_generar_demo(muestra_demo, s.n, s.x, referencia);
            muestra_demo = (muestra_demo + s.n) % FS_HZ;
        }
        else
        {
            if (xQueueReceive(cola_espectral, &s, pdMS_TO_TICKS(10)) != pdTRUE)
                continue;
            // Un cambio de modo mientras se esperaba se aplicara en la siguiente vuelta.
            if (leer_configuracion().version != c.version || s.version != c.version)
                continue;
            reiniciar |= s.id != id + 1 || s.epoca != epoca;
            id = s.id;
            epoca = s.epoca;
        }
        reiniciar |= n != 2 * s.n;
        if (reiniciar)
        {
            n = 2 * s.n;
            version = c.version;
            nodo3_iniciar(&p, n, c.paso ? 0 : c.umbral, c.ancho);
            vaciar_salida(1);
            portENTER_CRITICAL(&candado);
            estado.saltos = 0;
            estado.suma_us = 0;
            estado.max_us = 0;
            estado.muestras = 0;
            estado.tiene_referencia = false;
            estado.reinicios++;
            portEXIT_CRITICAL(&candado);
        }
        diagnostico_t d = s.diagnostico;
        bool falta_diagnostico = d.n_bloque != n;
        if (falta_diagnostico || c.paso)
            d = {RUIDO_NINGUNO, 0, (uint16_t)n, 0, 0, 0};
        uint32_t inicio = micros();
        int r = nodo3_procesar(&p, s.x, c.demo ? referencia : nullptr, y, &d, c.demo ? &bandas : nullptr);
        uint32_t tiempo = micros() - inicio;
        resultado_metricas_t resultados = {};
        bool medido = r == ASM_OK && metricas_calcular(&p.metricas, &resultados) == ASM_OK;
        portENTER_CRITICAL(&candado);
        if (configuracion.version != c.version)
        {
            portEXIT_CRITICAL(&candado);
            continue;
        }
        estado.n = n;
        estado.demo = c.demo;
        estado.paso = c.paso;
        estado.version_aplicada = c.version;
        if (r == ASM_OK)
        {
            estado.saltos++;
            estado.tiempo_us = tiempo;
            estado.suma_us += tiempo;
            if (tiempo > estado.max_us)
                estado.max_us = tiempo;
            if (falta_diagnostico)
                estado.sin_diagnostico++;
            estado.tiene_referencia = medido;
            estado.muestras = p.metricas.muestras;
            if (medido)
                estado.resultados = resultados;
        }
        else
            estado.errores++;
        portEXIT_CRITICAL(&candado);
        if (r == ASM_OK)
            encolar_salida(1, y, s.n, c.version);
        else
        {
            version = 0;
            vaciar_salida(1);
        }
        if (c.demo)
        {
            TickType_t periodo = pdMS_TO_TICKS((1000u * s.n) / FS_HZ);
            vTaskDelayUntil(&proxima, periodo ? periodo : 1);
        }
    }
}

static void imprimir_metrica(const char *nombre, float valor)
{
    Serial.print(nombre);
    Serial.print('=');
    if (isnan(valor))
        Serial.println("sin_medicion");
    else if (isinf(valor))
        Serial.println(valor > 0 ? "+inf" : "-inf");
    else
        Serial.println(valor, 6);
}

static void imprimir_estado()
{
    estado_t e;
    uint32_t ok, crc, perdidos, drops, drops_t, atrasados, vacios[2], llenos[2], recortes[2];
    unsigned pendientes[2];
    bool protocolo;
    portENTER_CRITICAL(&candado);
    e = estado;
    ok = tramas_ok;
    crc = errores_crc;
    perdidos = perdidas;
    drops = descartes;
    drops_t = descartes_temporal;
    atrasados = ticks_atrasados;
    protocolo = protocolo_pendiente;
    for (int i = 0; i < 2; i++)
    {
        vacios[i] = salidas[i].vacios;
        llenos[i] = salidas[i].llenos;
        recortes[i] = salidas[i].recortes;
        pendientes[i] = salidas[i].cantidad;
    }
    portEXIT_CRITICAL(&candado);
    configuracion_t c = leer_configuracion();
    Serial.printf("modo_solicitado=%s metodo_solicitado=%s N_solicitado=%d version=%lu aplicada=%lu\n",
                  c.demo ? "DEMO" : "VIVO", c.paso ? "PASO" : "ESPECTRAL", c.n,
                  (unsigned long)c.version, (unsigned long)e.version_aplicada);
    Serial.printf("modo_aplicado=%s metodo_aplicado=%s N_activo=%d umbral_solicitado=%.3f ancho_hz_solicitado=%.3f\n",
                  e.demo ? "DEMO" : "VIVO", e.paso ? "PASO" : "ESPECTRAL", e.n, c.umbral, c.ancho);
    Serial.printf("saltos=%lu errores=%lu reinicios=%lu saltos_sin_diagnostico=%lu\n",
                  (unsigned long)e.saltos, (unsigned long)e.errores, (unsigned long)e.reinicios, (unsigned long)e.sin_diagnostico);
    Serial.printf("tramas=%lu crc=%lu perdidas=%lu descartes_espectral=%lu descartes_temporal=%lu protocolo=%s\n",
                  (unsigned long)ok, (unsigned long)crc, (unsigned long)perdidos, (unsigned long)drops,
                  (unsigned long)drops_t, protocolo ? "PENDIENTE" : "disponible");
    Serial.println(c.demo ? "temporal=desactivado_en_demo" : "metricas_temporal=sin_referencia_sincronizada");
    Serial.printf("dac_frecuencia=%lu dac_bytes_por_s=%lu repetir=%s grabacion_s=%.1f\n",
                  (unsigned long)dac_frecuencia, (unsigned long)dac_bytes_por_s,
                  repitiendo < 0 ? "NO" : repitiendo == 2 ? "TONO" : repitiendo ? "ESPECTRAL" : "TEMPORAL", grab_n / (float)FS_HZ);
    Serial.printf("temporal_max_us=%lu temporal_redisenos=%lu temporal_tipo=%d\n",
                  (unsigned long)temporal_max_us, (unsigned long)temporal_redisenos, temporal_tipo);
    Serial.printf("temporal_codigo=%d trozos_DAC_atrasados=%lu memoria_nodo3_bytes=%u\n",
                  e.codigo_temporal, (unsigned long)atrasados, (unsigned)sizeof(nodo3_t));
    for (int i = 0; i < 2; i++)
        Serial.printf("DAC%d pendientes=%u vacios=%lu llenos=%lu recortes=%lu\n", i + 1, pendientes[i],
                      (unsigned long)vacios[i], (unsigned long)llenos[i], (unsigned long)recortes[i]);
    Serial.printf("referencia=%s muestras_evaluadas=%ld\n", e.tiene_referencia ? "DEMO_LOCAL" : "NO_DISPONIBLE", e.muestras);
    imprimir_metrica("MSE", e.tiene_referencia ? e.resultados.mse : NAN);
    imprimir_metrica("SNR_entrada_dB", e.tiene_referencia ? e.resultados.snr_entrada_db : NAN);
    imprimir_metrica("SNR_salida_dB", e.tiene_referencia ? e.resultados.snr_salida_db : NAN);
    imprimir_metrica("delta_SNR_dB", e.tiene_referencia ? e.resultados.delta_snr_db : NAN);
    imprimir_metrica("energia_util_pct", e.tiene_referencia ? e.resultados.energia_util_pct : NAN);
    imprimir_metrica("atenuacion_dB", e.tiene_referencia ? e.resultados.atenuacion_interferencia_db : NAN);
    imprimir_metrica("computo_medio_us", e.saltos ? e.suma_us / e.saltos : NAN);
    imprimir_metrica("computo_max_us", e.saltos ? e.max_us : NAN);
    imprimir_metrica("carga_computo_pct", e.saltos ? 100.0 * e.suma_us / e.saltos / (1e6 * e.n / (2 * FS_HZ)) : NAN);
}

// REPETIR TEMPORAL | ESPECTRAL | NO. Devuelve false si la linea no es un REPETIR.
static bool comando_repetir(const char *linea)
{
    if (strncmp(linea, "REPETIR", 7) != 0)
        return false;
    const char *arg = linea + 7;
    while (*arg == ' ')
        arg++;
    int canal = !strcmp(arg, "TEMPORAL") ? 0 : !strcmp(arg, "ESPECTRAL") ? 1 : !strcmp(arg, "TONO") ? 2
              : !strcmp(arg, "NO") ? -1 : -2;
    if (canal == -2)
        Serial.println("ERROR REPETIR TEMPORAL|ESPECTRAL|TONO|NO");
    else if ((canal == 0 || canal == 1) && !grab_n)
        Serial.println("ERROR no hay memoria para la grabacion");
    else
    {
        rep_pos = 0;   // desde lo mas viejo grabado
        repitiendo = canal;
        Serial.printf("OK REPETIR %s (%.1f s)\n", arg, grab_n / (float)FS_HZ);
    }
    return true;
}

static void ejecutar_comando(const char *linea)
{
    if (comando_repetir(linea))
        return;
    comando_nodo3_t orden;
    if (nodo3_leer_comando(linea, &orden) != ASM_OK)
    {
        Serial.println("ERROR comando; use AYUDA");
        return;
    }
    if (orden.orden == N3_ESTADO)
    {
        imprimir_estado();
        return;
    }
    if (orden.orden == N3_AYUDA)
    {
        Serial.println("ESTADO | DEMO | VIVO | REINICIAR | N 256/512/1024/2048");
        Serial.println("UMBRAL 0..1 | ANCHO 0..16000 | METODO ESPECTRAL/PASO");
        Serial.println("REPETIR TEMPORAL|ESPECTRAL|NO: toca por los dos DAC los ultimos segundos grabados");
        Serial.println("REPETIR TONO: 500 Hz por los dos DAC, para probar la salida");
        Serial.println("En VIVO, N se configura en la Pico; el ESP32 sigue el largo recibido.");
        return;
    }
    configuracion_t c = leer_configuracion();
    if (orden.orden == N3_N && !c.demo)
    {
        Serial.println("ERROR N: configure N en la Pico por USB; el comando UART inverso sigue reservado");
        return;
    }
    switch (orden.orden)
    {
    case N3_DEMO:
        c.demo = true;
        break;
    case N3_VIVO:
        c.demo = false;
        break;
    case N3_N:
        c.n = (int)orden.valor;
        break;
    case N3_UMBRAL:
        c.umbral = orden.valor;
        break;
    case N3_ANCHO:
        c.ancho = orden.valor;
        break;
    case N3_ESPECTRAL:
        c.paso = false;
        break;
    case N3_PASO:
        c.paso = true;
        break;
    default:
        break;
    }
    c.version++;
    portENTER_CRITICAL(&candado);
    configuracion = c;
    portEXIT_CRITICAL(&candado);
    xQueueReset(cola_espectral);
    xQueueReset(cola_temporal);
    vaciar_salida(0);
    vaciar_salida(1);
    Serial.printf("OK solicitado version=%lu; se aplica al siguiente salto\n", (unsigned long)c.version);
}

static void atender_usb()
{
    static char linea[96];
    static unsigned usados;
    static bool excedida;
    for (int i = 0; i < 128 && Serial.available(); i++)
    {
        char c = (char)Serial.read();
        if (c == '\r')
            continue;
        if (c == '\n')
        {
            if (excedida)
                Serial.println("ERROR linea demasiado larga");
            else if (usados)
            {
                linea[usados] = 0;
                ejecutar_comando(linea);
            }
            usados = 0;
            excedida = false;
        }
        else if (usados + 1 < sizeof linea && !excedida)
            linea[usados++] = c;
        else
            excedida = true;
    }
}

void setup()
{
    Serial.begin(115200);
    Serial2.setRxBufferSize(16384);
    Serial2.begin(BAUDIOS, SERIAL_8N1, ESP_PIN_UART_RX, ESP_PIN_UART_TX);
    trama_lector_iniciar(&lector);
    // Comprobar la implementacion del protocolo incluso antes de recibir bytes.
    protocolo_pendiente = trama_leer_byte(&lector, 0, &trama) == ASM_PENDIENTE;
    trama_lector_iniciar(&lector);
    estado.codigo_temporal = ASM_PENDIENTE;
    cola_temporal = xQueueCreate(4, sizeof(salto_t));
    cola_espectral = xQueueCreate(4, sizeof(salto_t));
    if (!cola_temporal || !cola_espectral)
    {
        Serial.println("ERROR memoria para colas");
        return;
    }
    if (!iniciar_dac())
    {
        Serial.println("ERROR DAC continuo");
        return;
    }
    if (xTaskCreatePinnedToCore(tarea_salida, "salida", 3072, nullptr, 5, nullptr, 0) != pdPASS ||
        xTaskCreatePinnedToCore(tarea_recepcion, "recepcion", 6144, nullptr, 3, nullptr, 0) != pdPASS ||
        xTaskCreatePinnedToCore(tarea_temporal, "temporal", 6144, nullptr, 2, nullptr, 1) != pdPASS ||
        xTaskCreatePinnedToCore(tarea_espectral, "espectral", 8192, nullptr, 2, nullptr, 1) != pdPASS)
    {
        Serial.println("ERROR creacion de tareas; reinicie la placa");
        return;
    }
    // Hasta 4 s por canal, con lo que sobre despues del DMA y las tareas. Se apartan
    // 20 KB mientras tanto para que la grabacion nunca deje al resto sin memoria.
    void *reserva[5];
    for (int i = 0; i < 5; i++)
        reserva[i] = malloc(BLOQUE_GRAB);
    unsigned bloques = 0;
    while (bloques < BLOQUES_GRAB)
    {
        uint8_t *a = (uint8_t *)malloc(BLOQUE_GRAB), *b = (uint8_t *)malloc(BLOQUE_GRAB);
        if (!a || !b)
        {
            free(a);
            free(b);
            break;
        }
        memset(a, 128, BLOQUE_GRAB);
        memset(b, 128, BLOQUE_GRAB);
        grabacion[0][bloques] = a;
        grabacion[1][bloques] = b;
        bloques++;
    }
    grab_n = bloques * BLOQUE_GRAB;
    for (int i = 0; i < 5; i++)
        free(reserva[i]);
    listo = true;
    Serial.println("Nodo 3 ready.");
}

void loop()
{
    if (listo)
        atender_usb();
    delay(1);
}

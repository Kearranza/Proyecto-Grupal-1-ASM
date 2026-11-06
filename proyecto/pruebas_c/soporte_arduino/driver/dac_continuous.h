// Doble del DAC continuo del ESP32 para test_interfaz: cada escritura avanza el reloj
// simulado lo que tardaria el DMA en consumirla, a frecuencia por canal.
#pragma once
#include "../Arduino.h"
typedef void *dac_continuous_handle_t;
enum { DAC_CHANNEL_MASK_ALL = 3 };
enum { DAC_DIGI_CLK_SRC_APLL = 1 };
enum { DAC_CHANNEL_MODE_ALTER = 1 };
typedef int esp_err_t;
#define ESP_OK 0
struct dac_continuous_config_t
{
    int chan_mask;
    uint32_t desc_num;
    size_t buf_size;
    uint32_t freq_hz;
    int8_t offset;
    int clk_src;
    int chan_mode;
};
inline uint32_t &dac_prueba_freq()
{
    static uint32_t f;
    return f;
}
inline esp_err_t dac_continuous_new_channels(const dac_continuous_config_t *c, dac_continuous_handle_t *h)
{
    dac_prueba_freq() = c->freq_hz;
    *h = reinterpret_cast<void *>(1);
    return ESP_OK;
}
inline esp_err_t dac_continuous_enable(dac_continuous_handle_t) { return ESP_OK; }
inline esp_err_t dac_continuous_disable(dac_continuous_handle_t) { return ESP_OK; }
inline esp_err_t dac_continuous_del_channels(dac_continuous_handle_t) { return ESP_OK; }
inline esp_err_t dac_continuous_write(dac_continuous_handle_t, uint8_t *, size_t n, size_t *cargados, int)
{
    reloj_prueba_us() += (uint32_t)(n * 1000000ull / (2ull * dac_prueba_freq()));
    if (cargados)
        *cargados = n;
    return ESP_OK;
}

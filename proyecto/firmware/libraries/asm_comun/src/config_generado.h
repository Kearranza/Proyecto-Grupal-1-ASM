// Generado por pc/generar_config_h.py desde config/sistema.json. No editar a mano.
#pragma once

#define FS_HZ 16000
#define N_POR_DEFECTO 1024
#define N_MAX 2048
#define BAUDIOS 921600
#define TRAMA_SYNC0 0xaa
#define TRAMA_SYNC1 0x55
#define TRAMA_TIPO_MUESTRAS 1
#define TRAMA_TIPO_DIAGNOSTICO 2
#define TRAMA_TIPO_COMANDO 3
#define TRAMA_TIPO_ESTADO 4
#define TRAMA_CARGA_MAX 4096
#define PICO_PIN_ADC 26
#define PICO_PIN_UART_TX 8
#define PICO_PIN_UART_RX 9
#define ESP_PIN_UART_RX 16
#define ESP_PIN_UART_TX 17
#define ESP_PIN_DAC_TEMPORAL 25
#define ESP_PIN_DAC_ESPECTRAL 26
#define ADC_BITS 12
#define DAC_BITS 8
#define DIAG_N_UTILES 3
#define DIAG_UTILES_HZ {440.0f, 880.0f, 1320.0f}
#define DIAG_ANCHO_UTIL_HZ 60.0f
#define SIM_N_TONOS 4
#define SIM_FRECUENCIAS_HZ {440, 880, 1320, 3500}
#define SIM_AMPLITUDES {1.0f, 0.55f, 0.3f, 0.9373f}
#define SIM_NIVEL_PICO 0.8f

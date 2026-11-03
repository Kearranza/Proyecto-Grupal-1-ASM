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
#define DIAG_FRECUENCIAS_UTILES_HZ {440, 880, 1320}
#define DIAG_N_UTILES 3
#define DIAG_MARGEN_BINS 2
#define DIAG_UMBRAL_NINGUNO_DB -30
#define DIAG_PLANITUD_BLANCO 0.5
#define DIAG_CONCENTRACION_TONAL 0.8
#define DIAG_CAIDA_BANDA_DB 20

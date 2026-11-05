// Sonda del enlace (Kevin). Convierte una Pico comun en el adaptador USB-TTL que
// necesita pc/registrar.py para grabar la evidencia de las pruebas.
//
// Escucha en GP9 la misma linea que el nodo 1 le manda al ESP32 por GP8 y vuelca
// esos bytes tal cual al USB. No transmite nada: el ESP32 sigue recibiendo igual.
//
// Cableado (ver docs/conexiones.md):
//   Pico W GP8 (pin 11)  ->  sonda GP9 (pin 12)     en paralelo con el ESP32
//   GND                  ->  GND                     obligatorio
//
// La sonda NO imprime texto por el USB a proposito. Se midio que el texto
// intercalado entre tramas no estorba, pero si parte una trama por la mitad se
// pierden todas: 0 de 40 recuperadas contra 40 de 40. El estado va por el LED.

#include <Arduino.h>
#include <asm_comun.h>

const uint8_t PIN_ESCUCHA = PICO_PIN_UART_RX;   // GP9, pin fisico 12
const uint8_t PIN_TX_LIBRE = 4;                 // UART1 TX a un pin sin conectar
const size_t  TROZO = 512;

static uint8_t buzon[TROZO];
static uint32_t bytes_vistos, ultimo_parpadeo;

void setup() {
    Serial.begin(115200);          // nominal: el USB de la Pico es nativo
    // El TX se manda a un pin suelto para no pelear con el GP8 del nodo 1
    Serial2.setTX(PIN_TX_LIBRE);
    Serial2.setRX(PIN_ESCUCHA);
    Serial2.setFIFOSize(4096);     // margen para los picos de 2 kB por bloque
    Serial2.begin(BAUDIOS);
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    int disponibles = Serial2.available();
    if (disponibles > 0) {
        size_t cuantos = Serial2.readBytes(buzon, min((size_t)disponibles, TROZO));
        Serial.write(buzon, cuantos);
        bytes_vistos += cuantos;
    }
    // LED fijo si esta llegando flujo, parpadeo lento si no llega nada
    uint32_t ahora = millis();
    if (ahora - ultimo_parpadeo >= 500) {
        ultimo_parpadeo = ahora;
        digitalWrite(LED_BUILTIN, bytes_vistos > 0 ? HIGH : !digitalRead(LED_BUILTIN));
        bytes_vistos = 0;
    }
}

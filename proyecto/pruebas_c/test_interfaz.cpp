// Prueba el sketch con perifericos simulados
#include "../firmware/nodos23_esp32/nodos23_esp32.ino"
#include <cassert>
#include <iostream>

int main()
{
    setup();
    assert(listo && protocolo_pendiente);
    uint32_t v = configuracion.version;
    ejecutar_comando("N 512");
    assert(configuracion.version == v && Serial.salida.find("configure N en la Pico") != std::string::npos);
    ejecutar_comando("DEMO");
    ejecutar_comando("N 512");
    ejecutar_comando("UMBRAL 0.25");
    ejecutar_comando("ANCHO 200");
    assert(configuracion.demo && configuracion.n == 512 && configuracion.umbral == 0.25f && configuracion.ancho == 200);
    ejecutar_comando("METODO PASO");
    assert(configuracion.paso);
    ejecutar_comando("METODO ESPECTRAL");
    assert(!configuracion.paso);
    v = configuracion.version;
    ejecutar_comando("UMBRAL nan");
    assert(configuracion.version == v);

    float x[128] = {0};
    x[0] = 2;
    x[1] = -2;
    x[2] = NAN;
    encolar_salida(1, x, 128, v);
    assert(salidas[1].cantidad == 128 && salidas[1].datos[0] == 255 && salidas[1].datos[1] == 0 && salidas[1].datos[2] == 128);
    ejecutar_comando("REINICIAR");
    assert(!salidas[1].activa && salidas[1].cantidad == 0);
    encolar_salida(1, x, 128, v);
    assert(salidas[1].cantidad == 0);
    for (int i = 0; i < 33; i++)
        encolar_salida(1, x, 128, configuracion.version);
    assert(salidas[1].llenos == 1 && salidas[1].cantidad == 128);

    Serial.salida.clear();
    Serial.entrada = std::string(110, 'X') + "\nESTADO\r\n";
    while (Serial.available())
        atender_usb();
    assert(Serial.salida.find("linea demasiado larga") != std::string::npos);
    assert(Serial.salida.find("MSE=sin_medicion") != std::string::npos);
    assert(Serial.salida.find("protocolo=PENDIENTE") != std::string::npos);
    ejecutar_comando("VIVO");
    assert(!configuracion.demo && !salidas[1].activa);
    std::cout << "[ok] interfaz, comandos, versiones, cola DAC y estado sin referencia (perifericos simulados)\n";
}

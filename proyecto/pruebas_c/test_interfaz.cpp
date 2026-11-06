// Prueba el sketch con perifericos simulados
#include "../firmware/nodos23_esp32/nodos23_esp32.ino"
#include <cassert>
#include <iostream>

int main()
{
    setup();
    // trama.c ya esta implementado: el protocolo tiene que quedar disponible
    assert(listo && !protocolo_pendiente);
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
    // La cola se recorta a tres saltos: nunca se desborda y no acumula atraso
    assert(salidas[1].llenos == 0 && salidas[1].cantidad == 384 && salidas[1].recortes == 30);
    assert(salidas[1].activa && salidas[1].cebando);

    Serial.salida.clear();
    Serial.entrada = std::string(110, 'X') + "\nESTADO\r\n";
    while (Serial.available())
        atender_usb();
    assert(Serial.salida.find("linea demasiado larga") != std::string::npos);
    assert(Serial.salida.find("MSE=sin_medicion") != std::string::npos);
    assert(Serial.salida.find("protocolo=disponible") != std::string::npos);
    ejecutar_comando("VIVO");
    assert(!configuracion.demo && !salidas[1].activa);
    // Grabar y repetir: 4 s por canal y solo con argumentos validos
    assert(grab_n == 4 * FS_HZ && repitiendo == -1);
    ejecutar_comando("REPETIR ESPECTRAL");
    assert(repitiendo == 1 && rep_pos == 0);
    ejecutar_comando("REPETIR TEMPORAL");
    assert(repitiendo == 0);
    Serial.salida.clear();
    ejecutar_comando("REPETIR OTRO");
    assert(repitiendo == 0 && Serial.salida.find("ERROR REPETIR") != std::string::npos);
    ejecutar_comando("REPETIR TONO");
    assert(repitiendo == 2);
    ejecutar_comando("REPETIR NO");
    assert(repitiendo == -1);
    std::cout << "[ok] interfaz, comandos, versiones, cola DAC y estado sin referencia (perifericos simulados)\n";
}

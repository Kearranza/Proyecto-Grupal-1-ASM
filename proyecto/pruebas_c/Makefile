# Compila los modulos de asm_comun con gcc y corre sus pruebas en la computadora.
#   make test          todas las pruebas
#   make test_fft      solo una
# Antes: python proyecto/pc/generar_config_h.py  y  python proyecto/pc/referencias_c.py
SRC_DIR := ../firmware/libraries/asm_comun/src
# M_PI no es de C11: _DEFAULT_SOURCE lo trae con glibc y _USE_MATH_DEFINES con MinGW
CFLAGS  := -std=c11 -O2 -Wall -Wextra -I$(SRC_DIR) -D_DEFAULT_SOURCE -D_USE_MATH_DEFINES
LDLIBS  := -lm
FUENTES := $(wildcard $(SRC_DIR)/*.c)
PRUEBAS := test_fft test_trama test_iir test_espectral test_metricas test_diagnostico test_nodo3

all: $(addprefix bin/,$(PRUEBAS))

bin/%: %.c prueba.h $(FUENTES) $(wildcard $(SRC_DIR)/*.h)
	@mkdir -p bin
	$(CC) $(CFLAGS) $< $(FUENTES) -o $@ $(LDLIBS)

$(PRUEBAS): %: bin/%
	@./bin/$@

test: all test_interfaz
	@fallas=0; for p in $(PRUEBAS); do ./bin/$$p || fallas=1; done; exit $$fallas

clean:
	rm -rf bin

.PHONY: all test clean $(PRUEBAS)

# El doble Arduino valida el flujo de interfaz, no la compatibilidad con el core.
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Isoporte_arduino -I$(SRC_DIR)
bin/%.o: $(SRC_DIR)/%.c $(wildcard $(SRC_DIR)/*.h)
	@mkdir -p bin
	$(CC) $(CFLAGS) -c $< -o $@

bin/test_interfaz: test_interfaz.cpp soporte_arduino/Arduino.h ../firmware/nodos23_esp32/nodos23_esp32.ino $(patsubst $(SRC_DIR)/%.c,bin/%.o,$(FUENTES))
	$(CXX) $(CXXFLAGS) $< $(patsubst $(SRC_DIR)/%.c,bin/%.o,$(FUENTES)) -o $@ $(LDLIBS)

test_interfaz: bin/test_interfaz
	@./bin/test_interfaz

.PHONY: test_interfaz

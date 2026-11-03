# Compila los modulos de asm_comun con gcc y corre sus pruebas en la computadora.
#   make test          todas las pruebas
#   make test_fft      solo una
# Antes: python proyecto/pc/generar_config_h.py  y  python proyecto/pc/referencias_c.py
SRC_DIR := ../firmware/libraries/asm_comun/src
CFLAGS  := -std=c11 -O2 -Wall -Wextra -I$(SRC_DIR) -D_DEFAULT_SOURCE
LDLIBS  := -lm
FUENTES := $(wildcard $(SRC_DIR)/*.c)
PRUEBAS := test_fft test_trama test_iir test_espectral test_metricas test_diagnostico

all: $(addprefix bin/,$(PRUEBAS))

bin/%: %.c prueba.h $(FUENTES) $(wildcard $(SRC_DIR)/*.h)
	@mkdir -p bin
	$(CC) $(CFLAGS) $< $(FUENTES) -o $@ $(LDLIBS)

$(PRUEBAS): %: bin/%
	@./bin/$@

test: all
	@fallas=0; for p in $(PRUEBAS); do ./bin/$$p || fallas=1; done; exit $$fallas

clean:
	rm -rf bin

.PHONY: all test clean $(PRUEBAS)

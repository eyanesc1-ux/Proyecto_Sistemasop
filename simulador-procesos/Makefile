# Makefile — Simulador de planificación (Fase 2, eimy)
#   make            compila ./simplanif (Linux: CLOUD-LIN02, APP-LIN01)
#   make ejecutar   compila y ejecuta todo (salida/)
#   make binarios   genera bin/linux/simplanif y bin/windows/simplanif.exe (estáticos)
#   make clean

CC     ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -D_DEFAULT_SOURCE
SRC    := src/kernel.c src/carga.c src/graficas.c src/main.c
MINGW  ?= x86_64-w64-mingw32-gcc

simplanif: $(SRC) include/simulador.h
	$(CC) $(CFLAGS) -Iinclude -o $@ $(SRC) -lm

ejecutar: simplanif
	./simplanif

binarios:
	mkdir -p bin/linux bin/windows
	$(CC) $(CFLAGS) -Iinclude -static -o bin/linux/simplanif $(SRC) -lm
	$(MINGW) $(CFLAGS) -Iinclude -static -o bin/windows/simplanif.exe $(SRC) -lm

clean:
	rm -rf simplanif salida

.PHONY: ejecutar binarios clean

# Physik auf dem Rechner pruefen, ohne Uhr und ohne Emulator.
#   make -C tools/hosttest run
CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter -std=c11 -I.
SRC = test_physics.c ../../src/c/physics.c ../../src/c/table.c ../../src/c/view.c

test_physics: $(SRC) ../../src/c/physics.h ../../src/c/table.h ../../src/c/fixed.h ../../src/c/view.h ../../src/c/config.h pebble_stub.h pebble.h
	$(CC) $(CFLAGS) -o $@ $(SRC) -lm

run: test_physics
	./test_physics

clean:
	rm -f test_physics

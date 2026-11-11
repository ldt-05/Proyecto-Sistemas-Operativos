CC = gcc
SRC = src/main.c src/cli.c src/scheduler.c

ifeq ($(OS),Windows_NT)
	CFLAGS = -std=c11 -Wall -Wextra -pthread -lmingw32 -mconsole
	OUT = simulador_so.exe
	TEST_OUT = test_motor.exe
else
	CFLAGS = -std=c11 -Wall -Wextra -pthread
	OUT = simulador_so
	TEST_OUT = test_motor
endif

.PHONY: all gui test clean

all:
	$(CC) $(SRC) -o $(OUT) $(CFLAGS)

gui:
	$(MAKE) -f gui/Makefile

test:
	$(CC) tests/test_motor.c -o $(TEST_OUT) $(CFLAGS)
	./$(TEST_OUT)

clean:
	rm -f simulador_so simulador_so.exe test_motor test_motor.exe
	$(MAKE) -f gui/Makefile clean

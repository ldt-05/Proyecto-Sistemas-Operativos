CC = gcc
CFLAGS = -Wall -Wextra -pthread -lmingw32 -mconsole
SRC = src/main.c src/cli.c src/scheduler.c
OUT = simulador_so.exe

all:
	$(CC) $(SRC) -o $(OUT) $(CFLAGS)

clean:
	rm -f $(OUT)
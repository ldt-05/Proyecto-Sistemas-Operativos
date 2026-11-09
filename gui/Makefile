CC ?= gcc

CLI_SRC ?= src/cli.c

CFLAGS ?= -std=c11 -O2 -Wall -Wextra -pthread

RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib)
RAYLIB_LIBS := $(shell pkg-config --libs raylib)

GUI_SRC = gui/gui_main.c \
          gui/gui.c \
          gui/gui_style.c \
          gui/gui_data.c \
          gui/gui_gantt.c

SRC = $(GUI_SRC) $(CLI_SRC) src/scheduler.c

OUT = gui/simulador_gui

.PHONY: all clean

all: $(OUT)

$(OUT): $(SRC) gui/gui.h gui/gui_style.h gui/gui_data.h gui/gui_gantt.h include/os_core.h include/scheduler.h
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) $(SRC) -o $(OUT) $(RAYLIB_LIBS) -lm

clean:
	rm -f $(OUT)

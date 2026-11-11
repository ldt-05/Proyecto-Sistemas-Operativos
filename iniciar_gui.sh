#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

if ! command -v gcc >/dev/null 2>&1; then
    echo "[ERROR] No se encontro gcc. Instala build-essential / gcc primero."
    exit 1
fi

SO_UNAME="$(uname -s 2>/dev/null || echo Unknown)"
ES_WINDOWS=0
case "$SO_UNAME" in
    MINGW*|MSYS*|CYGWIN*) ES_WINDOWS=1 ;;
esac

if [ "$ES_WINDOWS" -eq 0 ] && command -v make >/dev/null 2>&1; then
    echo "[1/2] Compilando version consola (simulador_so)..."
    make
    if pkg-config --exists raylib 2>/dev/null; then
        echo "[2/2] Compilando interfaz grafica Raylib (gui/simulador_gui)..."
        make gui
        echo "[INFO] Iniciando Simulador SO (CLI + Interfaz Grafica Raylib)..."
        exec ./gui/simulador_gui
    else
        echo "[AVISO] No se encontro raylib (pkg-config). Iniciando en modo consola..."
        exec ./simulador_so
    fi
fi

# Compilacion directa cuando make no esta disponible (ej. MSYS2 UCRT64) o en Windows Bash
CLI_FLAGS="-std=c11 -O2 -Wall -Wextra -pthread"
CLI_OUT="simulador_so"
GUI_OUT="gui/simulador_gui"
if [ "$ES_WINDOWS" -eq 1 ]; then
    CLI_FLAGS="$CLI_FLAGS -lmingw32 -mconsole"
    CLI_OUT="simulador_so.exe"
    GUI_OUT="gui/simulador_gui.exe"
fi

echo "[1/2] Compilando version consola ($CLI_OUT)..."
gcc src/main.c src/cli.c src/scheduler.c -o "$CLI_OUT" $CLI_FLAGS

if pkg-config --exists raylib 2>/dev/null; then
    echo "[2/2] Compilando interfaz grafica Raylib ($GUI_OUT)..."
    RAYLIB_CFLAGS="$(pkg-config --cflags raylib)"
    RAYLIB_LIBS="$(pkg-config --libs raylib)"
    gcc gui/gui_main.c gui/gui.c gui/gui_style.c gui/gui_data.c gui/gui_gantt.c src/cli.c src/scheduler.c \
        -o "$GUI_OUT" -std=c11 -O2 -Wall -Wextra -pthread $RAYLIB_CFLAGS $RAYLIB_LIBS -lm
    echo "[INFO] Iniciando Simulador SO (CLI + Interfaz Grafica Raylib)..."
    exec "./$GUI_OUT"
elif [ "$ES_WINDOWS" -eq 1 ] && [ -f "C:/msys64/ucrt64/include/raylib.h" ]; then
    echo "[2/2] Compilando interfaz grafica Raylib ($GUI_OUT)..."
    gcc gui/gui_main.c gui/gui.c gui/gui_style.c gui/gui_data.c gui/gui_gantt.c src/cli.c src/scheduler.c \
        -o "$GUI_OUT" -std=c11 -O2 -Wall -Wextra -pthread -IC:/msys64/ucrt64/include -LC:/msys64/ucrt64/lib -lraylib -lm
    echo "[INFO] Iniciando Simulador SO (CLI + Interfaz Grafica Raylib)..."
    exec "./$GUI_OUT"
else
    echo "[AVISO] No se encontro raylib. Iniciando en modo consola ($CLI_OUT)..."
    exec "./$CLI_OUT"
fi

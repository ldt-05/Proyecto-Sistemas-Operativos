#ifndef GUI_GANTT_H
#define GUI_GANTT_H

#include "raylib.h"
#include "gui_data.h"

/*
 * Dibuja el historial de ejecucion de la CPU
 * dentro del area indicada de la ventana.
 *
 * Los segmentos se obtienen del snapshot,
 * sin modificar los datos del planificador.
 */
void gui_dibujar_gantt(
    const GuiSnapshot* snapshot,
    Rectangle area
);

#endif

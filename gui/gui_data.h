#ifndef GUI_DATA_H
#define GUI_DATA_H

#include <stdbool.h>
#include "../include/os_core.h"
#include "../include/scheduler.h"

#define GUI_MAX_SEGMENTOS 256

/*
 * Representa una captura del estado del simulador
 * en un instante determinado.
 */
typedef struct {
    int tick_actual;
    int total_procesos;
    bool simulador_activo;

    // Copia de la tabla de procesos.
    PCB procesos[MAX_PROCESOS];

    // Historial de ejecucion de la CPU.
    int total_segmentos;
    GanttVista gantt[GUI_MAX_SEGMENTOS];

} GuiSnapshot;

/*
 * Obtiene una copia consistente de los datos
 * compartidos del simulador.
 */
bool gui_obtener_snapshot(GuiSnapshot* destino);

#endif

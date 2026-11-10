#include "gui_gantt.h"
#include "gui_style.h"

#include <stdio.h>

#define GANTT_TICKS_VISIBLES 32

// Asigna un color a cada proceso segun su PID.
static Color color_proceso(int pid) {
    const Color colores[] = {
        {74, 144, 226, 255},
        {73, 187, 143, 255},
        {169, 113, 218, 255},
        {224, 150, 69, 255},
        {214, 99, 119, 255},
        {55, 171, 189, 255}
    };

    if (pid < 0) {
        return (Color){91, 100, 116, 255};
    }

    return colores[(unsigned int)pid % 6];
}

/*
 * Representa graficamente el historial de CPU.
 *
 * El diagrama muestra los ultimos 32 ticks,
 * utilizando barras de colores para identificar
 * los procesos que ocuparon el procesador.
 */
void gui_dibujar_gantt(
    const GuiSnapshot* s,
    Rectangle area
) {
    DrawRectangleRec(
        area,
        (Color){25, 34, 51, 255}
    );

    gui_texto(
        "DIAGRAMA DE GANTT",
        area.x + 18.0f,
        area.y + 15.0f,
        21.0f,
        RAYWHITE
    );

    // Definir el intervalo temporal visible.
    int fin = s->tick_actual > GANTT_TICKS_VISIBLES
        ? s->tick_actual
        : GANTT_TICKS_VISIBLES;

    int inicio = fin - GANTT_TICKS_VISIBLES;

    float x0 = area.x + 22;
    float ancho = area.width - 44;
    float y0 = area.y + 65;
    float alto = 48;

    float por_tick = ancho / GANTT_TICKS_VISIBLES;

    // Dibujar el fondo de la linea de tiempo.
    DrawRectangle(
        (int)x0,
        (int)y0,
        (int)ancho,
        (int)alto,
        (Color){39, 48, 64, 255}
    );

    // Dibujar los segmentos registrados.
    for (int i = 0; i < s->total_segmentos; i++) {
        const GanttVista* g = &s->gantt[i];

        int desde = g->inicio > inicio
            ? g->inicio
            : inicio;

        int hasta = g->fin < fin
            ? g->fin
            : fin;

        // Ignorar segmentos fuera del intervalo visible.
        if (desde >= hasta) {
            continue;
        }

        float x = x0 + (desde - inicio) * por_tick;
        float w = (hasta - desde) * por_tick;

        DrawRectangle(
            (int)x,
            (int)y0 + 3,
            (int)(w + 0.5f),
            (int)alto - 6,
            color_proceso(g->pid)
        );

        // Mostrar el PID cuando hay espacio suficiente.
        if (w >= 38) {
            char nombre[24];

            if (g->pid == -1) {
                snprintf(
                    nombre,
                    sizeof(nombre),
                    "IDLE"
                );
            } else {
                snprintf(
                    nombre,
                    sizeof(nombre),
                    "P%d",
                    g->pid
                );
            }

            gui_texto(
                nombre,
                x + 5.0f,
                y0 + 14.0f,
                14.0f,
                RAYWHITE
            );
        }
    }

    // Dibujar marcas de la escala temporal.
    for (int tick = inicio; tick <= fin; tick += 4) {
        int x = (int)(
            x0 + (tick - inicio) * por_tick
        );

        DrawLine(
            x,
            (int)y0 + (int)alto + 3,
            x,
            (int)y0 + (int)alto + 10,
            (Color){118, 134, 155, 255}
        );

        gui_texto(
            TextFormat("%d", tick),
            (float)x - 7.0f,
            y0 + alto + 15.0f,
            13.0f,
            LIGHTGRAY
        );
    }

    // Mensaje inicial cuando no hay ejecucion.
    if (s->total_segmentos == 0) {
        gui_texto(
            "Sin ejecucion registrada",
            x0 + 12.0f,
            y0 + 15.0f,
            16.0f,
            LIGHTGRAY
        );
    }

    // Leyenda del diagrama.
    gui_texto(
        "P = PID  |  gris = CPU inactiva  |  ultimos 32 ticks",
        area.x + 18.0f,
        area.y + area.height - 27.0f,
        14.0f,
        (Color){155, 169, 187, 255}
    );
}

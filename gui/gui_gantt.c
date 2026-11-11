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

// Busca el nombre del proceso a partir de su PID en el snapshot.
static const char* nombre_por_pid(const GuiSnapshot* s, int pid) {
    for (int i = 0; i < s->total_procesos; i++) {
        if (s->procesos[i].pid == pid) {
            return s->procesos[i].nombre;
        }
    }
    return NULL;
}

// Intenta elegir texto y tamaño que quepan dentro del ancho de la barra.
static bool ajustar_etiqueta_barra(
    const char* nombre_proc,
    int pid,
    float ancho_barra,
    char* out_texto,
    size_t out_cap,
    float* out_size
) {
    const float espacio_util = ancho_barra - 4.0f;
    if (espacio_util <= 10.0f) {
        return false;
    }

    const float tamanos[] = {14.0f, 12.0f, 11.0f};

    if (pid == -1) {
        for (size_t i = 0; i < sizeof(tamanos) / sizeof(tamanos[0]); i++) {
            if (gui_texto_ancho("IDLE", tamanos[i]) <= espacio_util) {
                snprintf(out_texto, out_cap, "IDLE");
                *out_size = tamanos[i];
                return true;
            }
        }
        return false;
    }

    if (nombre_proc != NULL && nombre_proc[0] != '\0') {
        for (size_t i = 0; i < sizeof(tamanos) / sizeof(tamanos[0]); i++) {
            if (gui_texto_ancho(nombre_proc, tamanos[i]) <= espacio_util) {
                snprintf(out_texto, out_cap, "%s", nombre_proc);
                *out_size = tamanos[i];
                return true;
            }
        }
    }

    // Respaldo con P<pid> si el nombre completo es más ancho que la barra.
    char pid_txt[24];
    snprintf(pid_txt, sizeof(pid_txt), "P%d", pid);
    for (size_t i = 0; i < sizeof(tamanos) / sizeof(tamanos[0]); i++) {
        if (gui_texto_ancho(pid_txt, tamanos[i]) <= espacio_util) {
            snprintf(out_texto, out_cap, "%s", pid_txt);
            *out_size = tamanos[i];
            return true;
        }
    }

    return false;
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

    int hover_idx = -1;
    Vector2 mouse = GetMousePosition();

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
        Rectangle barra = {
            x,
            y0 + 3.0f,
            w,
            alto - 6.0f
        };

        DrawRectangleRec(barra, color_proceso(g->pid));

        if (CheckCollisionPointRec(mouse, barra)) {
            hover_idx = i;
            DrawRectangleLinesEx(barra, 2.0f, RAYWHITE);
        }

        const char* nombre_proc = (g->pid == -1)
            ? "IDLE"
            : nombre_por_pid(s, g->pid);

        char etiqueta[MAX_NOMBRE_LEN + 8];
        float tam_txt = 14.0f;

        if (ajustar_etiqueta_barra(
                nombre_proc,
                g->pid,
                w,
                etiqueta,
                sizeof(etiqueta),
                &tam_txt
            )) {
            float tw = gui_texto_ancho(etiqueta, tam_txt);
            float tx = x + (w - tw) / 2.0f;
            float ty = y0 + (alto - tam_txt) / 2.0f;

            gui_texto(
                etiqueta,
                tx,
                ty,
                tam_txt,
                RAYWHITE
            );
        }
    }

    if (hover_idx >= 0) {
        const GanttVista* g = &s->gantt[hover_idx];
        char detalle[96];

        if (g->pid == -1) {
            snprintf(
                detalle,
                sizeof(detalle),
                "IDLE | Ticks: %d - %d",
                g->inicio,
                g->fin
            );
        } else {
            const char* nombre_proc = nombre_por_pid(s, g->pid);
            snprintf(
                detalle,
                sizeof(detalle),
                "%s (PID %d) | Ticks: %d - %d",
                nombre_proc != NULL ? nombre_proc : "Proceso",
                g->pid,
                g->inicio,
                g->fin
            );
        }

        float dw = gui_texto_ancho(detalle, 15.0f);
        gui_texto(
            detalle,
            area.x + area.width - dw - 22.0f,
            area.y + 18.0f,
            15.0f,
            (Color){132, 196, 214, 255}
        );
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
        "Nombre o PID (si es angosto)  |  gris = CPU inactiva  |  ultimos 32 ticks  |  cursor sobre barra: detalle",
        area.x + 18.0f,
        area.y + area.height - 27.0f,
        14.0f,
        (Color){155, 169, 187, 255}
    );
}

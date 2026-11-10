#include "gui.h"
#include "gui_data.h"
#include "gui_gantt.h"
#include "gui_style.h"

#include "raylib.h"

#define ANCHO_INICIAL 1240
#define ALTO_INICIAL 800
#define ALTURA_FILA 32

static const Color FONDO = {14, 20, 32, 255};
static const Color PANEL = {25, 34, 51, 255};
static const Color LINEA = {52, 66, 87, 255};
static const Color TEXTO_SUAVE = {160, 176, 196, 255};

// Traduce el estado de un proceso a texto.
static const char* nombre_estado(ProcessState estado) {
    switch (estado) {
        case NUEVO: return "NEW";
        case LISTO: return "READY";
        case EJECUCION: return "RUNNING";
        case BLOQUEADO: return "BLOCKED";
        case FINALIZADO: return "TERMINATED";
        default: return "UNKNOWN";
    }
}

// Define los colores de los estados.
static Color color_estado(ProcessState estado) {
    switch (estado) {
        case NUEVO:
            return (Color){156, 170, 190, 255};
        case LISTO:
            return (Color){93, 162, 244, 255};
        case EJECUCION:
            return (Color){84, 203, 151, 255};
        case BLOQUEADO:
            return (Color){236, 183, 92, 255};
        case FINALIZADO:
            return (Color){171, 125, 234, 255};
        default:
            return TEXTO_SUAVE;
    }
}

/*
 * Dibuja el boton para cerrar la ventana grafica.
 * No modifica simulador_activo ni finaliza la CLI.
 */
static bool dibujar_boton_cerrar(int ancho) {
    Rectangle boton = {
        (float)(ancho - 180),
        26.0f,
        150.0f,
        40.0f
    };

    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, boton);

    Color fondo = hover
        ? (Color){180, 74, 96, 255}
        : (Color){141, 56, 75, 255};

    Color borde = hover
        ? (Color){235, 168, 181, 255}
        : (Color){203, 130, 147, 255};

    DrawRectangleRounded(boton, 0.22f, 10, fondo);
    DrawRectangleRoundedLinesEx(
        boton, 0.22f, 10, 2.0f, borde
    );

    const char* texto = "Cerrar GUI";
    float tw = gui_texto_ancho(texto, 18);

    gui_texto(
        texto,
        boton.x + (boton.width - tw) / 2.0f,
        boton.y + 10.0f,
        18.0f,
        RAYWHITE
    );

    return hover &&
        IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
}

/*
 * Calcula y representa los indicadores generales:
 * total, nuevos, listos, ejecutando y finalizados.
 */
static void dibujar_resumen(
    const GuiSnapshot* s,
    int ancho
) {
    int nuevos = 0;
    int listos = 0;
    int ejecucion = 0;
    int finalizados = 0;

    for (int i = 0; i < s->total_procesos; i++) {
        switch (s->procesos[i].estado) {
            case NUEVO: nuevos++; break;
            case LISTO: listos++; break;
            case EJECUCION: ejecucion++; break;
            case FINALIZADO: finalizados++; break;
            default: break;
        }
    }

    const int numeros[] = {
        s->total_procesos,
        nuevos,
        listos,
        ejecucion,
        finalizados
    };

    const char* nombres[] = {
        "TOTAL", "NEW", "READY", "RUNNING", "FINISHED"
    };

    int separacion = 10;
    int caja_ancho = (
        ancho - 48 - separacion * 4
    ) / 5;

    for (int i = 0; i < 5; i++) {
        int x = 24 + i * (caja_ancho + separacion);

        DrawRectangle(
            x, 104,
            caja_ancho, 77,
            PANEL
        );

        gui_texto(
            nombres[i],
            (float)x + 15.0f,
            116.0f,
            14.0f,
            TEXTO_SUAVE
        );

        gui_texto(
            TextFormat("%d", numeros[i]),
            (float)x + 15.0f,
            136.0f,
            30.0f,
            RAYWHITE
        );
    }
}

/*
 * Representa la tabla de procesos.
 * Permite desplazamiento vertical con el mouse.
 */
static void dibujar_tabla(
    const GuiSnapshot* s,
    Rectangle area,
    int* desplazamiento
) {
    DrawRectangleRec(area, PANEL);

    gui_texto(
        "PROCESOS",
        area.x + 18.0f,
        area.y + 12.0f,
        21.0f,
        RAYWHITE
    );

    const char* nombres[] = {
        "PID", "NOMBRE", "ESTADO", "AT",
        "BURST", "REST.", "PRIO.", "ESPERA"
    };

    int columnas[] = {
        18, 90, 280, 444,
        538, 635, 746, 860
    };

    int ycab = (int)area.y + 47;

    for (int i = 0; i < 8; i++) {
        gui_texto(
            nombres[i],
            area.x + (float)columnas[i],
            (float)ycab,
            14.0f,
            TEXTO_SUAVE
        );
    }

    DrawLine(
        (int)area.x + 16,
        ycab + 23,
        (int)(area.x + area.width - 16),
        ycab + 23,
        LINEA
    );

    // Calcular las filas visibles.
    int visibles = (
        (int)area.height - 80
    ) / ALTURA_FILA;

    if (visibles < 1) {
        visibles = 1;
    }

    int max_scroll = s->total_procesos > visibles
        ? s->total_procesos - visibles
        : 0;

    if (*desplazamiento > max_scroll) {
        *desplazamiento = max_scroll;
    }

    // Permitir desplazamiento con la rueda del mouse.
    if (CheckCollisionPointRec(GetMousePosition(), area)) {
        float giro = GetMouseWheelMove();

        if (giro > 0) *desplazamiento -= 2;
        if (giro < 0) *desplazamiento += 2;

        if (*desplazamiento < 0) {
            *desplazamiento = 0;
        }

        if (*desplazamiento > max_scroll) {
            *desplazamiento = max_scroll;
        }
    }

    if (s->total_procesos == 0) {
        gui_texto(
            "Sin procesos. Usa 'create' o 'load' desde la CLI.",
            area.x + 20.0f,
            (float)ycab + 55.0f,
            18.0f,
            TEXTO_SUAVE
        );
        return;
    }

    // Dibujar los procesos visibles.
    for (
        int fila = 0;
        fila < visibles &&
        fila + *desplazamiento < s->total_procesos;
        fila++
    ) {
        const PCB* p = &s->procesos[
            fila + *desplazamiento
        ];

        int y = ycab + 31 + fila * ALTURA_FILA;

        // Fondo alternado para facilitar la lectura.
        if (fila % 2 == 0) {
            DrawRectangle(
                (int)area.x + 12,
                y - 5,
                (int)area.width - 24,
                ALTURA_FILA,
                (Color){33, 43, 62, 255}
            );
        }

        gui_texto(
            TextFormat("%d", p->pid),
            area.x + (float)columnas[0],
            (float)y, 15.0f, RAYWHITE
        );

        gui_texto(
            p->nombre,
            area.x + (float)columnas[1],
            (float)y, 15.0f, RAYWHITE
        );

        gui_texto(
            nombre_estado(p->estado),
            area.x + (float)columnas[2],
            (float)y, 15.0f,
            color_estado(p->estado)
        );

        gui_texto(
            TextFormat("%d", p->tiempo_llegada),
            area.x + (float)columnas[3],
            (float)y, 15.0f, RAYWHITE
        );

        gui_texto(
            TextFormat("%d", p->burst_inicial),
            area.x + (float)columnas[4],
            (float)y, 15.0f, RAYWHITE
        );

        gui_texto(
            TextFormat("%d", p->burst_restante),
            area.x + (float)columnas[5],
            (float)y, 15.0f, RAYWHITE
        );

        gui_texto(
            TextFormat("%d", p->prioridad),
            area.x + (float)columnas[6],
            (float)y, 15.0f, RAYWHITE
        );

        gui_texto(
            TextFormat("%d", p->tiempo_espera),
            area.x + (float)columnas[7],
            (float)y, 15.0f, RAYWHITE
        );
    }

    // Mostrar informacion sobre el desplazamiento.
    if (max_scroll > 0) {
        gui_texto(
            TextFormat(
                "Filas %d-%d de %d (rueda para desplazar)",
                *desplazamiento + 1,
                *desplazamiento + visibles < s->total_procesos
                    ? *desplazamiento + visibles
                    : s->total_procesos,
                s->total_procesos
            ),
            area.x + 18.0f,
            area.y + area.height - 21.0f,
            13.0f,
            TEXTO_SUAVE
        );
    }
}

/*
 * Ejecuta el ciclo principal de la ventana.
 *
 * La interfaz consulta periodicamente el snapshot
 * mientras el motor y la CLI operan en otros hilos.
 */
void gui_ejecutar(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    InitWindow(
        ANCHO_INICIAL,
        ALTO_INICIAL,
        "Simulador SO - Monitor de Procesos"
    );

    gui_estilo_inicializar();

    SetWindowMinSize(1100, 680);
    SetTargetFPS(30);

    int desplazamiento = 0;
    GuiSnapshot snapshot;
    bool salir = false;

    while (!WindowShouldClose() && !salir) {
        if (!gui_obtener_snapshot(&snapshot) ||
            !snapshot.simulador_activo) {
            break;
        }

        int ancho = GetScreenWidth();
        int alto = GetScreenHeight();
        int gantt_y = alto - 233;

        BeginDrawing();
        ClearBackground(FONDO);

        gui_texto(
            "MONITOR DE PROCESOS",
            24.0f, 22.0f, 30.0f,
            RAYWHITE
        );

        gui_texto(
            "Simulador de Sistemas Operativos | Visualizacion en tiempo real",
            25.0f, 66.0f, 17.0f,
            TEXTO_SUAVE
        );

        gui_texto(
            "La CLI sigue disponible en la terminal",
            25.0f, 84.0f, 14.0f,
            (Color){132, 196, 214, 255}
        );

        gui_texto(
            TextFormat("TICK %d", snapshot.tick_actual),
            (float)(ancho - 320),
            29.0f, 28.0f,
            (Color){84, 203, 151, 255}
        );

        if (dibujar_boton_cerrar(ancho)) {
            salir = true;
        }

        dibujar_resumen(&snapshot, ancho);

        dibujar_tabla(
            &snapshot,
            (Rectangle){
                24, 194,
                (float)ancho - 48,
                (float)gantt_y - 206
            },
            &desplazamiento
        );

        gui_dibujar_gantt(
            &snapshot,
            (Rectangle){
                24,
                (float)gantt_y,
                (float)ancho - 48,
                192
            }
        );

        gui_texto(
            "Control desde CLI: create, load, start, pause, set_sched, set_speed, exit",
            26.0f,
            (float)alto - 24.0f,
            14.0f,
            TEXTO_SUAVE
        );

        EndDrawing();
    }

    gui_estilo_cerrar();
    CloseWindow();
}

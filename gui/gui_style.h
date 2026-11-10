#ifndef GUI_STYLE_H
#define GUI_STYLE_H

#include "raylib.h"

/*
 * Inicializa la fuente utilizada por la interfaz.
 * Debe llamarse despues de InitWindow().
 */
void gui_estilo_inicializar(void);

/*
 * Libera los recursos de la fuente.
 * Debe llamarse antes de CloseWindow().
 */
void gui_estilo_cerrar(void);

/*
 * Dibuja un texto utilizando la fuente de la GUI.
 */
void gui_texto(
    const char* texto,
    float x,
    float y,
    float size,
    Color color
);

/*
 * Calcula el ancho de un texto para facilitar
 * su posicionamiento dentro de la interfaz.
 */
float gui_texto_ancho(
    const char* texto,
    float size
);

#endif

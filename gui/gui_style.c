#include "gui_style.h"

#include <stdbool.h>
#include <stddef.h>

// Fuente compartida por los componentes de la GUI.
static Font fuente_ui;
static bool fuente_personalizada = false;

/*
 * Busca una fuente legible en el sistema operativo.
 *
 * Se prioriza DejaVu Sans en Linux y se incluyen
 * alternativas para otros sistemas.
 */
void gui_estilo_inicializar(void) {
    const char* rutas[] = {
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
        "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
        "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf"
    };

    fuente_ui = GetFontDefault();
    fuente_personalizada = false;

    for (size_t i = 0; i < sizeof(rutas) / sizeof(rutas[0]); i++) {
        if (!FileExists(rutas[i])) {
            continue;
        }

        Font candidata = LoadFontEx(
            rutas[i],
            32,
            NULL,
            0
        );

        if (candidata.texture.id != 0) {
            fuente_ui = candidata;
            fuente_personalizada = true;

            SetTextureFilter(
                fuente_ui.texture,
                TEXTURE_FILTER_BILINEAR
            );

            break;
        }
    }
}

/*
 * Libera la fuente personalizada cuando
 * se cierra la interfaz grafica.
 */
void gui_estilo_cerrar(void) {
    if (fuente_personalizada) {
        UnloadFont(fuente_ui);
    }
}

/*
 * Dibuja texto utilizando la fuente seleccionada.
 *
 * Si no se encontro ninguna fuente del sistema,
 * utiliza la fuente predeterminada de raylib.
 */
void gui_texto(
    const char* texto,
    float x,
    float y,
    float size,
    Color color
) {
    if (fuente_personalizada) {
        DrawTextEx(
            fuente_ui,
            texto,
            (Vector2){x, y},
            size,
            1.0f,
            color
        );
    } else {
        DrawText(
            texto,
            (int)x,
            (int)y,
            (int)size,
            color
        );
    }
}

/*
 * Calcula el ancho de un texto para
 * posicionarlo correctamente en pantalla.
 */
float gui_texto_ancho(
    const char* texto,
    float size
) {
    if (fuente_personalizada) {
        return MeasureTextEx(
            fuente_ui,
            texto,
            size,
            1.0f
        ).x;
    }

    return (float)MeasureText(
        texto,
        (int)size
    );
}

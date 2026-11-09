#include "gui_data.h"

#include <pthread.h>
#include <string.h>

/*
 * Obtiene una copia consistente del estado del simulador.
 *
 * La captura se realiza bajo mutex_sistema para evitar
 * lecturas inconsistentes mientras el motor actualiza
 * los procesos y el historial de Gantt.
 */
bool gui_obtener_snapshot(GuiSnapshot* destino) {

    if (destino == NULL) {
        return false;
    }

    // Inicializar la estructura de destino.
    memset(destino, 0, sizeof(*destino));

    // Bloquear las estructuras compartidas.
    pthread_mutex_lock(&mutex_sistema);

    // Capturar el estado general.
    destino->tick_actual = clock_ticks;
    destino->simulador_activo = simulador_activo;

    int total = num_procesos;

    // Limitar la cantidad a la capacidad del arreglo.
    if (total < 0) {
        total = 0;
    }

    if (total > MAX_PROCESOS) {
        total = MAX_PROCESOS;
    }

    destino->total_procesos = total;

    // Copiar los bloques de control de procesos.
    if (total > 0) {
        memcpy(
            destino->procesos,
            procesos,
            (size_t)total * sizeof(PCB)
        );
    }

    // Copiar los segmentos recientes del Gantt.
    // motor_copiar_gantt() espera que el mutex
    // ya se encuentre bloqueado.
    destino->total_segmentos = motor_copiar_gantt(
        destino->gantt,
        GUI_MAX_SEGMENTOS
    );

    // Liberar el acceso a las estructuras compartidas.
    pthread_mutex_unlock(&mutex_sistema);

    return true;
}

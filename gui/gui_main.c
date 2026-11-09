#include "gui.h"
#include "../include/os_core.h"
#include "../include/scheduler.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>

// Estado compartido del simulador.
PCB procesos[MAX_PROCESOS];
int num_procesos = 0;
bool simulador_activo = true;
pthread_mutex_t mutex_sistema;

// clock_ticks se encuentra definido en scheduler.c.

int main(void) {
    pthread_t hilo_cli;

    // Inicializar el mecanismo de sincronizacion.
    if (pthread_mutex_init(&mutex_sistema, NULL) != 0) {
        fprintf(stderr, "No se pudo iniciar el mutex.\n");
        return 1;
    }

    // Iniciar el hilo del motor de planificacion.
    if (!motor_iniciar()) {
        fprintf(stderr, "No se pudo iniciar el motor.\n");
        pthread_mutex_destroy(&mutex_sistema);
        return 1;
    }

    // Iniciar el hilo de la interfaz de comandos.
    if (pthread_create(
            &hilo_cli,
            NULL,
            HiloCLI,
            NULL
        ) != 0) {

        fprintf(stderr, "No se pudo iniciar la CLI.\n");

        pthread_mutex_lock(&mutex_sistema);
        simulador_activo = false;
        pthread_mutex_unlock(&mutex_sistema);

        motor_finalizar();
        pthread_mutex_destroy(&mutex_sistema);

        return 1;
    }

    puts("[GUI] Interfaz grafica iniciada.");
    puts("[GUI] Puedes seguir usando la CLI en esta misma terminal.");

    // Ejecutar la ventana en el hilo principal.
    gui_ejecutar();

    // Comprobar si el simulador continua activo.
    pthread_mutex_lock(&mutex_sistema);
    bool activo = simulador_activo;
    pthread_mutex_unlock(&mutex_sistema);

    if (activo) {
        puts(
            "[GUI] La ventana fue cerrada. "
            "Escribe 'exit' en la terminal para finalizar."
        );
    }

    // Esperar el cierre de la CLI.
    pthread_join(hilo_cli, NULL);

    // Finalizar el motor y liberar los recursos.
    motor_finalizar();
    pthread_mutex_destroy(&mutex_sistema);

    return 0;
}

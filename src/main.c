#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "../include/os_core.h"

// ==========================================
// DEFINICIÓN DE VARIABLES GLOBALES
// ==========================================
PCB procesos[MAX_PROCESOS];
int num_procesos = 0;
bool simulador_activo = true;
int clock_ticks = 0;
pthread_mutex_t mutex_sistema;

int main() {
    pthread_t hilo_cli;

    // Inicializar el Mutex principal
    if (pthread_mutex_init(&mutex_sistema, NULL) != 0) {
        printf("[ERROR FATAL]: Fallo al inicializar el Mutex del sistema.\n");
        return 1;
    }

    // 1. Crear el hilo interactivo de la CLI (Responsabilidad de Luis)
    if (pthread_create(&hilo_cli, NULL, HiloCLI, NULL) != 0) {
        printf("[ERROR FATAL]: No se pudo crear el hilo CLI.\n");
        pthread_mutex_destroy(&mutex_sistema);
        return 1;
    }

    // (Aquí Renzo conectará su Hilo Motor de CPU en el siguiente módulo)

    // Esperar a que el hilo CLI termine (cuando el usuario escribe 'exit')
    pthread_join(hilo_cli, NULL);

    // Destrucción limpia de recursos del sistema
    pthread_mutex_destroy(&mutex_sistema);

    printf("Sistema apagado correctamente. Recursos liberados.\n");
    return 0;
}

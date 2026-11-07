#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "../include/os_core.h"
#include "../include/scheduler.h"

// ==========================================
// DEFINICIÓN DE VARIABLES GLOBALES
// ==========================================
PCB procesos[MAX_PROCESOS];
int num_procesos = 0;
bool simulador_activo = true;
pthread_mutex_t mutex_sistema;

int main() {
    pthread_t hilo_cli;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // Soporte de acentos y caracteres UTF-8 en PowerShell / CMD
#endif

    // Inicializar el Mutex principal
    if (pthread_mutex_init(&mutex_sistema, NULL) != 0) {
        printf("[ERROR FATAL]: Fallo al inicializar el Mutex del sistema.\n");
        return 1;
    }

    // 1. Crear el hilo interactivo de la CLI (Luis)
    if (pthread_create(&hilo_cli, NULL, HiloCLI, NULL) != 0) {
        printf("[ERROR FATAL]: No se pudo crear el hilo CLI.\n");
        pthread_mutex_destroy(&mutex_sistema);
        return 1;
    }

    // 2. Crear el hilo del Motor de CPU (Renzo)
    if (!motor_iniciar()) {
        printf("[ERROR FATAL]: No se pudo crear el hilo Motor de CPU.\n");
        pthread_mutex_lock(&mutex_sistema);
        simulador_activo = false;
        pthread_mutex_unlock(&mutex_sistema);
        pthread_join(hilo_cli, NULL);
        pthread_mutex_destroy(&mutex_sistema);
        return 1;
    }

    // Esperar a que el hilo CLI termine (cuando el usuario escribe 'exit')
    pthread_join(hilo_cli, NULL);

    // Detener y liberar el motor de CPU
    motor_finalizar();

    // Destrucción limpia de recursos del sistema
    pthread_mutex_destroy(&mutex_sistema);

    printf("Sistema apagado correctamente. Recursos liberados.\n");
    return 0;
}

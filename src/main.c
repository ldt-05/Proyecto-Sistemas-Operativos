#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "../include/os_core.h"
#include "../include/scheduler.h"

// Definición de las variables globales
PCB procesos[100];
int num_procesos = 0;
bool simulador_activo = true;
pthread_mutex_t mutex_sistema;

// Declaración de la función externa del CLI
extern void* HiloCLI(void* arg);

int main() {
    pthread_t hilo_cli;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // Para que la consola muestre bien los acentos
#endif

    // Inicializar el Mutex principal
    if (pthread_mutex_init(&mutex_sistema, NULL) != 0) {
        printf("Error fatal: Fallo al inicializar el Mutex.\n");
        return 1;
    }

    // 1. Crear el hilo de la CLI
    if (pthread_create(&hilo_cli, NULL, HiloCLI, NULL) != 0) {
        printf("Error fatal: No se pudo crear el hilo CLI.\n");
        return 1;
    }

    // 2. Crear el hilo del Motor de CPU
    if (!motor_iniciar()) {
        printf("Error fatal: No se pudo crear el hilo Motor.\n");
        return 1;
    }

    // Esperar a que el hilo CLI termine (cuando el usuario escribe 'exit')
    pthread_join(hilo_cli, NULL);
    motor_finalizar();

    // Liberar recursos
    pthread_mutex_destroy(&mutex_sistema);

    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "../include/os_core.h"

// Función del Hilo CLI
void* HiloCLI(void* arg) {
    char input[256];
    char* comando;

    printf("=== Shell del Simulador de SO ===\n");
    printf("Escribe 'help' para ver los comandos o 'exit' para salir.\n\n");

    while (simulador_activo) {
        printf("SO-Shell> ");
        if (fgets(input, sizeof(input), stdin) == NULL) continue;

        // Remover el salto de línea
        input[strcspn(input, "\n")] = 0;

        if (strlen(input) == 0) continue;

        // Parsear el primer argumento (el comando base)
        comando = strtok(input, " ");

        if (strcmp(comando, "help") == 0) {
            printf("\n--- MANUAL DE USUARIO ---\n");
            printf("create <nombre> <burst> [prioridad] : Crea un proceso nuevo.\n");
            printf("ps                                  : Muestra el estado de procesos.\n");
            printf("pcb <PID>                           : Detalle del PCB.\n");
            printf("exit                                : Cierra la simulacion limpia.\n\n");
        } 
        else if (strcmp(comando, "exit") == 0) {
            // Protección al salir para que los hilos cierren bien
            pthread_mutex_lock(&mutex_sistema);
            simulador_activo = false;
            pthread_mutex_unlock(&mutex_sistema);
            printf("Cerrando el sistema de forma limpia...\n");
        } 
        else if (strcmp(comando, "ps") == 0) {
            // Ejemplo de acceso protegido para lectura
            pthread_mutex_lock(&mutex_sistema);
            if (num_procesos == 0) {
                printf("No hay procesos en el sistema.\n");
            } else {
                printf("PID\tNOMBRE\t\tESTADO\n");
                // TODO: Iterar e imprimir procesos
            }
            pthread_mutex_unlock(&mutex_sistema);
        }
        else {
            // Validación estricta: No colapsar si el comando no existe
            printf("[ERROR DE SINTAXIS]: Comando '%s' no reconocido.\n", comando);
        }
    }
    
    return NULL;
}
#ifndef OS_CORE_H
#define OS_CORE_H

#include <pthread.h>
#include <stdbool.h>

// Estados del proceso definidos en la rúbrica
typedef enum {
    NUEVO,
    LISTO,
    EJECUCION,
    BLOQUEADO,
    FINALIZADO
} ProcessState;

// Process Control Block (PCB)
typedef struct {
    int pid;
    char nombre[20];
    ProcessState estado;
    int burst_inicial;
    int burst_restante;
    int prioridad;
    int tiempo_llegada;
    int tiempo_espera;
    int tiempo_retorno;
    int tiempo_respuesta;
    int tiempo_finalizacion;
    bool ha_iniciado;   // Ya paso por la CPU (para el tiempo de respuesta)
    int orden_listo;    // Posicion en la cola de LISTOS (el menor sale primero)
} PCB;

// Variables Globales Compartidas (Declaración)
extern PCB procesos[100]; // Arreglo estático temporal como cola
extern int num_procesos;
extern bool simulador_activo; // Bandera para el Hilo CLI
extern int clock_ticks;       // Reloj simulado (definido en scheduler.c)

// Mutex para proteger la lectura y escritura de las estructuras compartidas
extern pthread_mutex_t mutex_sistema;

#endif
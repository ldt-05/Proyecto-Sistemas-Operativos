#ifndef OS_CORE_H
#define OS_CORE_H

#include <pthread.h>
#include <stdbool.h>

#define MAX_PROCESOS 100
#define MAX_NOMBRE_LEN 20

// ==========================================
// ESTADOS DEL PROCESO
// ==========================================
typedef enum {
    NUEVO,
    LISTO,
    EJECUCION,
    BLOQUEADO,
    FINALIZADO
} ProcessState;

// ==========================================
// PROCESS CONTROL BLOCK (PCB)
// ==========================================
typedef struct {
    int pid;                            // Identificador único (1, 2, 3...)
    char nombre[MAX_NOMBRE_LEN];        // Nombre del proceso sin espacios
    ProcessState estado;                // Estado actual en el ciclo de vida
    int burst_inicial;                  // Ráfaga de CPU original solicitada
    int burst_restante;                 // Ráfaga que le falta consumir
    int prioridad;                      // Prioridad (1: más alta, 5: más baja)
    int tiempo_llegada;                 // Tick en el que ingresó al sistema (AT)
    int tiempo_espera;                  // Ticks acumulados esperando en cola LISTO (WT)
    int tiempo_retorno;                 // TAT = CT - AT (solo cuando finaliza)
    int tiempo_respuesta;               // RT = primer_tick_cpu - AT
    int tiempo_finalizacion;            // Tick en el que burst_restante llegó a 0 (CT)
    int tiempo_inicio;                  // Tick de la primera vez en CPU
    bool ha_iniciado;                   // Indica si ya pisó la CPU alguna vez
    int orden_listo;                    // Posición en la cola de LISTOS para desempate FIFO
} PCB;

// ==========================================
// VARIABLES GLOBALES COMPARTIDAS
// ==========================================
extern PCB procesos[MAX_PROCESOS];      // Tabla global de procesos
extern int num_procesos;                // Cantidad total de procesos registrados
extern bool simulador_activo;           // Control de vida del sistema (true = activo)
extern int clock_ticks;                 // Reloj maestro simulado de la CPU
extern pthread_mutex_t mutex_sistema;   // Candado de exclusión mutua

// ==========================================
// PROTOTIPOS DE FUNCIONES BASE
// ==========================================
// Función principal del Hilo de la Interfaz de Usuario (Luis)
void* HiloCLI(void* arg);

// Función utilitaria para convertir el enum ProcessState a texto legible
const char* estado_a_string(ProcessState estado);

#endif // OS_CORE_H

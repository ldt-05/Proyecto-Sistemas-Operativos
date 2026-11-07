#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include "../include/os_core.h"
#include "../include/scheduler.h"

#define MAX_LINE_LEN 256
#define MAX_TOKENS 16

// ============================================================================
// CONVERSIÓN DE ENUM A TEXTO
// ============================================================================
const char* estado_a_string(ProcessState estado) {
    switch (estado) {
        case NUEVO:      return "NEW";
        case LISTO:      return "READY";
        case EJECUCION:  return "RUNNING";
        case BLOQUEADO:  return "BLOCKED";
        case FINALIZADO: return "TERMINATED";
        default:         return "UNKNOWN";
    }
}

// ============================================================================
// FUNCIONES AUXILIARES DE PARSING ROBUSTO (A PRUEBA DE FALLOS)
// ============================================================================

// Convierte un string en entero de forma segura usando strtol
static bool parse_int(const char* str, int* out_val) {
    if (str == NULL || *str == '\0') {
        return false;
    }
    char* endptr = NULL;
    long val = strtol(str, &endptr, 10);

    // Si endptr apunta a algo distinto del fin de cadena, contenía letras o símbolos
    if (*endptr != '\0') {
        return false;
    }

    // Verificar desbordamiento de enteros
    if (val > INT_MAX || val < INT_MIN) {
        return false;
    }

    *out_val = (int)val;
    return true;
}

// Tokeniza una línea de entrada dividiendo por espacios y tabuladores
static int tokenizar_linea(char* linea, char* tokens[], int max_tokens) {
    int count = 0;
    char* token = strtok(linea, " \t\r\n");
    while (token != NULL && count < max_tokens) {
        tokens[count++] = token;
        token = strtok(NULL, " \t\r\n");
    }
    return count;
}

// ============================================================================
// COMANDOS DE LUIS (40%)
// ============================================================================

// 1. Comando: create <nombre> <burst> [prioridad]
static void ejecutar_create(int token_count, char* tokens[]) {
    if (token_count < 3) {
        printf("[ERROR DE SINTAXIS]: Parámetros insuficientes. Uso correcto: create <nombre> <burst_time> [prioridad]\n");
        return;
    }
    if (token_count > 4) {
        printf("[ERROR DE SINTAXIS]: Demasiados parámetros. Uso correcto: create <nombre> <burst_time> [prioridad]\n");
        return;
    }

    const char* nombre = tokens[1];
    if (strlen(nombre) >= MAX_NOMBRE_LEN) {
        printf("[ERROR DE VALIDACION]: El nombre del proceso excede el límite de %d caracteres.\n", MAX_NOMBRE_LEN - 1);
        return;
    }

    int burst = 0;
    if (!parse_int(tokens[2], &burst) || burst <= 0) {
        printf("[ERROR DE TIPO]: El burst_time debe ser un número entero mayor a 0.\n");
        return;
    }

    int prioridad = 3; // Prioridad por defecto según la especificación
    if (token_count == 4) {
        if (!parse_int(tokens[3], &prioridad)) {
            printf("[ERROR DE TIPO]: La prioridad debe ser un número entero entre 1 y 5.\n");
            return;
        }
        if (prioridad < 1 || prioridad > 5) {
            printf("[ERROR DE RANGO]: La prioridad debe estar en el rango de 1 a 5.\n");
            return;
        }
    }

    // Sección Crítica: Inserción en la tabla de procesos compartida
    pthread_mutex_lock(&mutex_sistema);

    if (num_procesos >= MAX_PROCESOS) {
        printf("[ERROR DEL SISTEMA]: Capacidad máxima de procesos alcanzada (%d procesos).\n", MAX_PROCESOS);
        pthread_mutex_unlock(&mutex_sistema);
        return;
    }

    static int siguiente_pid = 1; // Generador de PID autoincremental

    int idx = num_procesos;
    procesos[idx].pid = siguiente_pid++;
    strncpy(procesos[idx].nombre, nombre, MAX_NOMBRE_LEN - 1);
    procesos[idx].nombre[MAX_NOMBRE_LEN - 1] = '\0';
    procesos[idx].estado = LISTO;
    procesos[idx].burst_inicial = burst;
    procesos[idx].burst_restante = burst;
    procesos[idx].prioridad = prioridad;
    procesos[idx].tiempo_llegada = clock_ticks;
    procesos[idx].tiempo_espera = 0;
    procesos[idx].tiempo_retorno = -1;
    procesos[idx].tiempo_respuesta = -1;
    procesos[idx].ha_iniciado = false;
    procesos[idx].tiempo_inicio = -1;
    procesos[idx].tiempo_finalizacion = -1;

    num_procesos++;

    printf("[OK]: Proceso '%s' creado con PID %d en t=%d ticks (Burst: %d, Prioridad: %d, Estado: READY).\n",
           procesos[idx].nombre, procesos[idx].pid, procesos[idx].tiempo_llegada, burst, prioridad);

    pthread_mutex_unlock(&mutex_sistema);
}

// 2. Comando: ps
static void ejecutar_ps(int token_count) {
    if (token_count > 1) {
        printf("[ERROR DE SINTAXIS]: El comando 'ps' no recibe parámetros.\n");
        return;
    }

    // Sección Crítica: Lectura coherente de la tabla de procesos
    pthread_mutex_lock(&mutex_sistema);

    if (num_procesos == 0) {
        printf("No hay procesos en el sistema.\n");
    } else {
        printf("%-5s %-12s %-12s %-16s %-10s\n", "PID", "NOMBRE", "ESTADO", "BURST RESTANTE", "PRIORIDAD");
        for (int i = 0; i < num_procesos; i++) {
            char burst_fmt[16];
            snprintf(burst_fmt, sizeof(burst_fmt), "%ds", procesos[i].burst_restante);
            printf("%-5d %-12s %-12s %-16s %-10d\n",
                   procesos[i].pid,
                   procesos[i].nombre,
                   estado_a_string(procesos[i].estado),
                   burst_fmt,
                   procesos[i].prioridad);
        }
    }

    pthread_mutex_unlock(&mutex_sistema);
}

// 3. Comando: pcb <PID>
static void ejecutar_pcb(int token_count, char* tokens[]) {
    if (token_count < 2) {
        printf("[ERROR DE SINTAXIS]: Parámetros insuficientes. Uso correcto: pcb <PID>\n");
        return;
    }
    if (token_count > 2) {
        printf("[ERROR DE SINTAXIS]: Demasiados parámetros. Uso correcto: pcb <PID>\n");
        return;
    }

    int target_pid = 0;
    if (!parse_int(tokens[1], &target_pid) || target_pid <= 0) {
        printf("[ERROR DE TIPO]: El PID debe ser un número entero positivo.\n");
        return;
    }

    // Sección Crítica: Búsqueda del PCB
    pthread_mutex_lock(&mutex_sistema);

    int encontrado_idx = -1;
    for (int i = 0; i < num_procesos; i++) {
        if (procesos[i].pid == target_pid) {
            encontrado_idx = i;
            break;
        }
    }

    if (encontrado_idx == -1) {
        printf("[ERROR]: No se encontró ningún proceso con PID %d.\n", target_pid);
        pthread_mutex_unlock(&mutex_sistema);
        return;
    }

    PCB* p = &procesos[encontrado_idx];

    // Formatear tiempos condicionales
    char retorno_str[32];
    if (p->estado == FINALIZADO) {
        snprintf(retorno_str, sizeof(retorno_str), "%d ticks", p->tiempo_retorno);
    } else {
        snprintf(retorno_str, sizeof(retorno_str), "N/A (En ejecución)");
    }

    char respuesta_str[32];
    if (p->ha_iniciado) {
        snprintf(respuesta_str, sizeof(respuesta_str), "%d ticks", p->tiempo_respuesta);
    } else {
        snprintf(respuesta_str, sizeof(respuesta_str), "N/A (No iniciado)");
    }

    printf("========================================\n");
    printf("       CONTROL BLOCK DE PROCESO (PCB)   \n");
    printf("========================================\n");
    printf("PID                 : %d\n", p->pid);
    printf("Nombre del Proceso  : %s\n", p->nombre);
    printf("Estado Actual       : %s\n", estado_a_string(p->estado));
    printf("Prioridad           : %d\n", p->prioridad);
    printf("Tiempo de Llegada   : %d ticks\n", p->tiempo_llegada);
    printf("Ráfaga Inicial      : %d ticks\n", p->burst_inicial);
    printf("Ráfaga Restante     : %d ticks\n", p->burst_restante);
    printf("Tiempo de Espera    : %d ticks\n", p->tiempo_espera);
    printf("Tiempo de Retorno   : %s\n", retorno_str);
    printf("Tiempo de Respuesta : %s\n", respuesta_str);
    printf("========================================\n");

    pthread_mutex_unlock(&mutex_sistema);
}

// 4. Comando: help
static void ejecutar_help(int token_count) {
    if (token_count > 1) {
        printf("[ERROR DE SINTAXIS]: El comando 'help' no recibe parámetros.\n");
        return;
    }

    printf("\n================ MANUAL DE COMANDOS DEL SISTEMA ================\n");
    printf("  create <nom> <burst> [prio] : Crea un nuevo proceso (Prioridad: 1 a 5).\n");
    printf("  load <archivo.txt>          : Carga procesos por lotes desde un archivo CSV.\n");
    printf("  ps                          : Muestra la tabla de estados de los procesos.\n");
    printf("  pcb <PID>                   : Muestra el detalle del PCB de un proceso.\n");
    printf("  set_sched <RR|SRTF> [q]     : Configura el algoritmo (RR con quantum opcional).\n");
    printf("  set_speed <ms>              : Configura velocidad del reloj (50 a 5000 ms).\n");
    printf("  start                       : Inicia o reanuda la simulación de CPU.\n");
    printf("  pause                       : Pausa la ejecución de la CPU.\n");
    printf("  gantt                       : Imprime el diagrama de Gantt acumulado.\n");
    printf("  stats                       : Muestra métricas globales del sistema.\n");
    printf("  help                        : Muestra este manual de ayuda.\n");
    printf("  exit                        : Detiene y cierra el simulador limpiamente.\n");
    printf("=================================================================\n\n");
}

// 5. Comando: exit
static void ejecutar_exit(int token_count) {
    if (token_count > 1) {
        printf("[ERROR DE SINTAXIS]: El comando 'exit' no recibe parámetros.\n");
        return;
    }

    pthread_mutex_lock(&mutex_sistema);
    simulador_activo = false;
    pthread_mutex_unlock(&mutex_sistema);

    printf("Cerrando el sistema de forma limpia...\n");
}

// ============================================================================
// HILO PRINCIPAL DE LA CLI (INTERFAZ DE USUARIO)
// ============================================================================
void* HiloCLI(void* arg) {
    (void)arg; // Evita advertencias de compilador sobre parámetro no utilizado
    char linea[MAX_LINE_LEN];
    char* tokens[MAX_TOKENS];

    printf("\n==================================================\n");
    printf("  Simulador de Kernel y Gestor de Procesos (CLI)  \n");
    printf("  Fase I - Escribe 'help' para ver los comandos.  \n");
    printf("==================================================\n\n");

    while (true) {
        // Verificar de forma segura si el simulador sigue activo
        pthread_mutex_lock(&mutex_sistema);
        bool activo = simulador_activo;
        pthread_mutex_unlock(&mutex_sistema);

        if (!activo) {
            break;
        }

        printf("SO-Shell> ");
        fflush(stdout);

        if (fgets(linea, sizeof(linea), stdin) == NULL) {
            // Manejo de EOF (Ctrl+D / Ctrl+Z)
            pthread_mutex_lock(&mutex_sistema);
            simulador_activo = false;
            pthread_mutex_unlock(&mutex_sistema);
            break;
        }

        // Tokenización estricta
        int token_count = tokenizar_linea(linea, tokens, MAX_TOKENS);

        // Si la línea está vacía (solo dio Enter o espacios), volver al prompt silenciosamente
        if (token_count == 0) {
            continue;
        }

        char* cmd = tokens[0];

        // Despacho de Comandos asignados a Luis (40%)
        if (strcmp(cmd, "create") == 0) {
            ejecutar_create(token_count, tokens);
        } else if (strcmp(cmd, "ps") == 0) {
            ejecutar_ps(token_count);
        } else if (strcmp(cmd, "pcb") == 0) {
            ejecutar_pcb(token_count, tokens);
        } else if (strcmp(cmd, "help") == 0) {
            ejecutar_help(token_count);
        } else if (strcmp(cmd, "exit") == 0) {
            ejecutar_exit(token_count);
            break;
        }
        // Identificadores de comandos reservados para Renzo y Juan
        else if (strcmp(cmd, "load") == 0 ||
                 strcmp(cmd, "set_sched") == 0 ||
                 strcmp(cmd, "set_speed") == 0 ||
                 strcmp(cmd, "start") == 0 ||
                 strcmp(cmd, "pause") == 0 ||
                 strcmp(cmd, "gantt") == 0 ||
                 strcmp(cmd, "stats") == 0) {
            printf("[MODULO PENDIENTE]: El comando '%s' pertenece al módulo de Planificación / Lotes (Renzo / Juan).\n", cmd);
        } else {
            // Validación estricta ante comandos desconocidos
            printf("[ERROR DE SINTAXIS]: Comando '%s' no reconocido. Escribe 'help' para ver comandos válidos.\n", cmd);
        }
    }

    return NULL;
}

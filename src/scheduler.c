// Hilo Motor de CPU: reloj simulado, algoritmos RR y SRTF, metricas y Gantt.
//
// Cada tick tiene tres pasos:
//   1. (con mutex)  admitir llegadas y decidir quien usa la CPU.
//   2. (sin mutex)  dormir el retardo real, para que la CLI siga respondiendo.
//   3. (con mutex)  consumir la rafaga, avanzar el reloj y actualizar metricas.
//
// El reloj solo avanza si hay trabajo. Sin procesos pendientes se queda quieto,
// asi el Gantt no acumula IDLE al final y el uso de CPU no se distorsiona.

#define _DEFAULT_SOURCE
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../include/os_core.h"
#include "../include/scheduler.h"

#define ESPERA_POLL_US 5000
#define SEPARADORES " \t\r"
#define GANTT_IDLE -1
#define SIN_PROCESO -1

typedef enum { ALGORITMO_RR, ALGORITMO_SRTF } Algoritmo;

// Intervalo continuo del Gantt. indice es la posicion en procesos[] o GANTT_IDLE.
typedef struct {
    int indice;
    int inicio;
    int fin;
} SegmentoGantt;

int clock_ticks = 0;

// Todo este estado se protege con mutex_sistema.
static Algoritmo algoritmo = ALGORITMO_RR;
static int quantum = 2;
static int velocidad_ms = 1000;
static bool motor_corriendo = false;
static int actual = SIN_PROCESO;   // Indice del proceso en CPU
static int quantum_usado = 0;
static int ticks_ocupados = 0;     // Ticks con un proceso en CPU, para el % de uso
static int ultimo_orden = 0;       // Numera las entradas a LISTO: el menor es el primero de la cola

static SegmentoGantt* gantt = NULL;
static int gantt_len = 0;
static int gantt_cap = 0;

static pthread_t hilo_motor;

// ---------------------------- Planificacion ----------------------------

static void encolar_listo(int i) {
    procesos[i].estado = LISTO;
    procesos[i].orden_listo = ++ultimo_orden;
}

// NUEVO -> LISTO cuando el reloj alcanza la llegada. Si un proceso se creo a mitad
// de un tick entra un tick despues de su llegada; ese tick ya cuenta como espera.
static void admitir_llegadas(void) {
    for (int i = 0; i < num_procesos; i++) {
        if (procesos[i].estado == NUEVO && procesos[i].tiempo_llegada <= clock_ticks) {
            procesos[i].tiempo_espera += clock_ticks - procesos[i].tiempo_llegada;
            encolar_listo(i);
        }
    }
}

// Primero de la cola (RR) o menor rafaga restante, con empate por orden de llegada (SRTF).
static int elegir_siguiente(void) {
    int elegido = SIN_PROCESO;
    for (int i = 0; i < num_procesos; i++) {
        if (procesos[i].estado != LISTO) continue;
        if (elegido == SIN_PROCESO) {
            elegido = i;
            continue;
        }
        int rafaga_i = procesos[i].burst_restante;
        int rafaga_e = procesos[elegido].burst_restante;
        bool mas_corto = algoritmo == ALGORITMO_SRTF && rafaga_i < rafaga_e;
        bool empata = algoritmo == ALGORITMO_RR || rafaga_i == rafaga_e;
        if (mas_corto || (empata && procesos[i].orden_listo < procesos[elegido].orden_listo)) {
            elegido = i;
        }
    }
    return elegido;
}

// Desaloja al proceso en CPU si la politica activa lo exige.
static void aplicar_expropiacion(int siguiente) {
    if (actual == SIN_PROCESO) return;

    if (algoritmo == ALGORITMO_RR && quantum_usado >= quantum) {
        if (siguiente == SIN_PROCESO) {
            quantum_usado = 0;     // Nadie espera: continua con un quantum nuevo
        } else {
            encolar_listo(actual); // Va al final de la cola, despues de las llegadas de este tick
            actual = SIN_PROCESO;
        }
    } else if (algoritmo == ALGORITMO_SRTF && siguiente != SIN_PROCESO &&
               procesos[siguiente].burst_restante < procesos[actual].burst_restante) {
        // Solo si es estrictamente menor: un empate no justifica un cambio de contexto.
        encolar_listo(actual);
        actual = SIN_PROCESO;
    }
}

static void despachar(void) {
    if (actual != SIN_PROCESO) return;

    actual = elegir_siguiente();
    if (actual == SIN_PROCESO) return;

    PCB* p = &procesos[actual];
    p->estado = EJECUCION;
    quantum_usado = 0;
    if (!p->ha_iniciado) {
        p->ha_iniciado = true;
        p->tiempo_respuesta = clock_ticks - p->tiempo_llegada;
    }
}

static bool hay_llegadas_pendientes(void) {
    for (int i = 0; i < num_procesos; i++) {
        if (procesos[i].estado == NUEVO) return true;
    }
    return false;
}

// ------------------------------- Gantt ---------------------------------

// Agrega el tick [inicio, inicio + 1) y lo une al ultimo intervalo si es del mismo proceso.
static void gantt_registrar(int indice, int inicio) {
    if (gantt_len > 0 && gantt[gantt_len - 1].indice == indice) {
        gantt[gantt_len - 1].fin = inicio + 1;
        return;
    }
    if (gantt_len == gantt_cap) {
        int nueva_cap = gantt_cap == 0 ? 32 : gantt_cap * 2;
        SegmentoGantt* nuevo = realloc(gantt, (size_t)nueva_cap * sizeof(SegmentoGantt));
        if (nuevo == NULL) {
            fprintf(stderr, "[ERROR]: Sin memoria para el historial de Gantt.\n");
            return;
        }
        gantt = nuevo;
        gantt_cap = nueva_cap;
    }
    gantt[gantt_len++] = (SegmentoGantt){indice, inicio, inicio + 1};
}

// ----------------------------- Hilo Motor ------------------------------

static long long ahora_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

// Duerme un tick sin tener el mutex. El tiempo en pausa no cuenta, asi que pause congela
// el tick a la mitad, y set_speed y exit surten efecto de inmediato.
// Devuelve false si el simulador se esta cerrando.
static bool esperar_tick(void) {
    long long transcurrido = 0;
    long long anterior = ahora_ms();

    for (;;) {
        usleep(ESPERA_POLL_US);
        long long ahora = ahora_ms();

        pthread_mutex_lock(&mutex_sistema);
        bool activo = simulador_activo;
        bool corriendo = motor_corriendo;
        int duracion = velocidad_ms;
        pthread_mutex_unlock(&mutex_sistema);

        if (!activo) return false;
        if (corriendo) transcurrido += ahora - anterior;
        if (transcurrido >= duracion) return true;
        anterior = ahora;
    }
}

static void ejecutar_tick(void) {
    for (int i = 0; i < num_procesos; i++) {
        if (procesos[i].estado == LISTO) procesos[i].tiempo_espera++;
    }

    gantt_registrar(actual == SIN_PROCESO ? GANTT_IDLE : actual, clock_ticks);

    if (actual != SIN_PROCESO) {
        procesos[actual].burst_restante--;
        ticks_ocupados++;
        quantum_usado++;
    }

    clock_ticks++;

    if (actual != SIN_PROCESO && procesos[actual].burst_restante == 0) {
        PCB* p = &procesos[actual];
        p->estado = FINALIZADO;
        p->tiempo_finalizacion = clock_ticks;
        p->tiempo_retorno = p->tiempo_finalizacion - p->tiempo_llegada;
        actual = SIN_PROCESO;
    }
}

static void* HiloMotor(void* arg) {
    (void)arg;

    for (;;) {
        bool hay_tick = false;

        pthread_mutex_lock(&mutex_sistema);
        bool activo = simulador_activo;
        if (activo && motor_corriendo) {
            admitir_llegadas();
            aplicar_expropiacion(elegir_siguiente());
            despachar();
            hay_tick = actual != SIN_PROCESO || hay_llegadas_pendientes();
        }
        pthread_mutex_unlock(&mutex_sistema);

        if (!activo) break;
        if (!hay_tick) {
            usleep(ESPERA_POLL_US);
            continue;
        }
        if (!esperar_tick()) break;

        pthread_mutex_lock(&mutex_sistema);
        ejecutar_tick();
        pthread_mutex_unlock(&mutex_sistema);
    }
    return NULL;
}

bool motor_iniciar(void) {
    return pthread_create(&hilo_motor, NULL, HiloMotor, NULL) == 0;
}

void motor_finalizar(void) {
    pthread_join(hilo_motor, NULL);
    free(gantt);
    gantt = NULL;
    gantt_len = 0;
    gantt_cap = 0;
}

// ------------------------------ Comandos -------------------------------

static bool parsear_entero(const char* texto, int* valor) {
    char* fin;
    errno = 0;
    long n = strtol(texto, &fin, 10);
    if (*texto == '\0' || *fin != '\0' || errno == ERANGE || n < INT_MIN || n > INT_MAX) {
        return false;
    }
    *valor = (int)n;
    return true;
}

static const char* siguiente_arg(void) {
    return strtok(NULL, SEPARADORES);
}

static void cmd_set_sched(void) {
    const char* nombre = siguiente_arg();
    const char* texto_quantum = siguiente_arg();
    const char* uso = "Uso correcto: set_sched <RR|SRTF> [quantum]";

    if (nombre == NULL) {
        printf("[ERROR DE SINTAXIS]: Parámetros insuficientes.\n%s\n", uso);
        return;
    }
    if (siguiente_arg() != NULL) {
        printf("[ERROR DE SINTAXIS]: Demasiados parámetros.\n%s\n", uso);
        return;
    }

    Algoritmo nuevo;
    int nuevo_quantum = 2;
    if (strcmp(nombre, "RR") == 0) {
        nuevo = ALGORITMO_RR;
        if (texto_quantum != NULL) {
            if (!parsear_entero(texto_quantum, &nuevo_quantum)) {
                printf("[ERROR DE TIPO]: El quantum debe ser un número entero.\n");
                return;
            }
            if (nuevo_quantum <= 0) {
                printf("[ERROR DE PARAMETRO]: El quantum debe ser mayor que 0.\n");
                return;
            }
        }
    } else if (strcmp(nombre, "SRTF") == 0) {
        nuevo = ALGORITMO_SRTF;
        if (texto_quantum != NULL) {
            printf("[ERROR DE SINTAXIS]: SRTF no usa quantum.\n%s\n", uso);
            return;
        }
    } else {
        printf("[ERROR DE PARAMETRO]: Algoritmo no reconocido. Algoritmos válidos: RR, SRTF.\n");
        return;
    }

    pthread_mutex_lock(&mutex_sistema);
    algoritmo = nuevo;
    quantum = nuevo_quantum;
    quantum_usado = 0;
    pthread_mutex_unlock(&mutex_sistema);

    if (nuevo == ALGORITMO_RR) {
        printf("Planificador: RR (quantum = %d).\n", nuevo_quantum);
    } else {
        printf("Planificador: SRTF.\n");
    }
}

static void cmd_set_speed(void) {
    const char* texto = siguiente_arg();
    const char* uso = "Uso correcto: set_speed <ms_por_tick>";

    if (texto == NULL) {
        printf("[ERROR DE SINTAXIS]: Parámetros insuficientes.\n%s\n", uso);
        return;
    }
    if (siguiente_arg() != NULL) {
        printf("[ERROR DE SINTAXIS]: Demasiados parámetros.\n%s\n", uso);
        return;
    }

    int ms;
    if (!parsear_entero(texto, &ms)) {
        printf("[ERROR DE TIPO]: La velocidad debe ser un número entero.\n");
        return;
    }
    if (ms < 50 || ms > 5000) {
        printf("[ERROR DE PARAMETRO]: La velocidad debe estar entre 50 y 5000 ms.\n");
        return;
    }

    pthread_mutex_lock(&mutex_sistema);
    velocidad_ms = ms;
    pthread_mutex_unlock(&mutex_sistema);
    printf("Velocidad: %d ms por tick.\n", ms);
}

static void cmd_start(void) {
    pthread_mutex_lock(&mutex_sistema);
    bool ya_corria = motor_corriendo;
    motor_corriendo = true;
    pthread_mutex_unlock(&mutex_sistema);
    printf(ya_corria ? "El motor ya está en ejecución.\n" : "Motor en ejecución.\n");
}

static void cmd_pause(void) {
    pthread_mutex_lock(&mutex_sistema);
    bool corria = motor_corriendo;
    motor_corriendo = false;
    int tick = clock_ticks;
    pthread_mutex_unlock(&mutex_sistema);

    if (corria) {
        printf("Motor en pausa (tick %d).\n", tick);
    } else {
        printf("El motor ya está en pausa.\n");
    }
}

static void cmd_gantt(void) {
    pthread_mutex_lock(&mutex_sistema);
    printf("DIAGRAMA DE GANTT (0 - %d ticks):\n", clock_ticks);
    if (gantt_len == 0) {
        printf("(sin ejecución todavía)\n");
    }
    for (int i = 0; i < gantt_len; i++) {
        const SegmentoGantt* s = &gantt[i];
        const char* nombre = s->indice == GANTT_IDLE ? "IDLE" : procesos[s->indice].nombre;
        printf("%s[%d-%ds: %s]", i > 0 ? " -> " : "", s->inicio, s->fin, nombre);
    }
    if (gantt_len > 0) printf("\n");
    pthread_mutex_unlock(&mutex_sistema);
}

static void cmd_stats(void) {
    pthread_mutex_lock(&mutex_sistema);
    int finalizados = 0;
    int iniciados = 0;
    long suma_espera = 0;
    long suma_retorno = 0;
    long suma_respuesta = 0;

    for (int i = 0; i < num_procesos; i++) {
        if (procesos[i].estado == FINALIZADO) {
            finalizados++;
            suma_espera += procesos[i].tiempo_espera;
            suma_retorno += procesos[i].tiempo_retorno;
        }
        if (procesos[i].ha_iniciado) {
            iniciados++;
            suma_respuesta += procesos[i].tiempo_respuesta;
        }
    }

    // Espera y retorno promedian los finalizados; respuesta, los que ya tocaron la CPU.
    printf("ESTADÍSTICAS DEL SISTEMA:\n");
    printf("- Procesos Finalizados       : %d / %d\n", finalizados, num_procesos);
    printf("- Tiempo Promedio de Espera  : %.2f ticks\n",
           finalizados ? (double)suma_espera / finalizados : 0.0);
    printf("- Tiempo Promedio de Retorno : %.2f ticks\n",
           finalizados ? (double)suma_retorno / finalizados : 0.0);
    printf("- Tiempo Promedio de Respuesta: %.2f ticks\n",
           iniciados ? (double)suma_respuesta / iniciados : 0.0);
    printf("- Uso de CPU                 : %.1f%%\n",
           clock_ticks ? 100.0 * ticks_ocupados / clock_ticks : 0.0);
    pthread_mutex_unlock(&mutex_sistema);
}

// Para los comandos sin parametros: ejecuta solo si no sobran argumentos.
static void sin_argumentos(const char* comando, void (*accion)(void)) {
    if (siguiente_arg() != NULL) {
        printf("[ERROR DE SINTAXIS]: El comando '%s' no recibe parámetros.\n", comando);
        return;
    }
    accion();
}

bool comando_motor(const char* comando) {
    if (strcmp(comando, "set_sched") == 0) cmd_set_sched();
    else if (strcmp(comando, "set_speed") == 0) cmd_set_speed();
    else if (strcmp(comando, "start") == 0) sin_argumentos(comando, cmd_start);
    else if (strcmp(comando, "pause") == 0) sin_argumentos(comando, cmd_pause);
    else if (strcmp(comando, "gantt") == 0) sin_argumentos(comando, cmd_gantt);
    else if (strcmp(comando, "stats") == 0) sin_argumentos(comando, cmd_stats);
    else return false;
    return true;
}

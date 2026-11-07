#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdbool.h>

// Crea el Hilo Motor de CPU. Devuelve false si no se pudo crear.
bool motor_iniciar(void);

// Espera a que el Hilo Motor termine (simulador_activo == false) y libera su memoria.
void motor_finalizar(void);

// Encola un proceso en estado LISTO asignándole orden secuencial para desempate FIFO
void motor_encolar_listo(int indice_proceso);

// Atiende los comandos del motor: set_sched, set_speed, start, pause, gantt, stats
// Recibe los tokens ya separados por la CLI para garantizar un parsing estricto y seguro
// Devuelve true si el comando fue reconocido (incluso si tuvo errores de sintaxis/parámetro)
bool comando_motor(int token_count, char* tokens[]);

#endif // SCHEDULER_H

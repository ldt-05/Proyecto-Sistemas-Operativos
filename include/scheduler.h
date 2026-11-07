#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdbool.h>

// Crea el Hilo Motor. Devuelve false si no se pudo crear.
bool motor_iniciar(void);

// Espera a que el Hilo Motor termine (simulador_activo == false) y libera su memoria.
void motor_finalizar(void);

// Atiende set_sched, set_speed, start, pause, gantt y stats.
// Se llama despues de strtok(input, " "): los argumentos se leen con strtok(NULL, ...).
// Devuelve false si el comando no es del motor.
bool comando_motor(const char* comando);

#endif

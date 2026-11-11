# Simulador de Kernel y Gestor de Procesos (Fase I)

Simulador concurrente en lenguaje **C11** con hilos POSIX (`pthread.h`) y exclusión mutua (`pthread_mutex_t`) que integra una consola interactiva (**Hilo CLI**), un motor de planificación de CPU (**Hilo Motor**) con políticas **Round Robin (RR)** y **SRTF** con expropiación, carga por lotes CSV y una interfaz gráfica opcional en **raylib**.

---

## Arquitectura Modular

* `include/os_core.h`: Definición de estados (`NUEVO`, `LISTO`, `EJECUCION`, `BLOQUEADO`, `FINALIZADO`), estructura `PCB`, variables compartidas y `mutex_sistema`.
* `include/scheduler.h` / `src/scheduler.c`: Motor de CPU discreto (`clock_ticks`), algoritmos `RR` y `SRTF`, expropiación, métricas (`WT`, `TAT`, `RT`, uso de CPU) y registro de Gantt dinámico.
* `src/cli.c`: Hilo CLI con tokenizador estricto y parser seguro (`strtol`), comandos `create`, `load`, `ps`, `pcb`, `help`, `exit` y despacho al motor (`set_sched`, `set_speed`, `start`, `pause`, `gantt`, `stats`).
* `src/main.c`: Punto de entrada en modo consola; inicializa el mutex y coordina ambos hilos concurrentes.
* `gui/`: Interfaz gráfica opcional en tiempo real usando `raylib` (`make -f gui/Makefile`).
* `tests/`: Batería de pruebas unitarias y de concurrencia (`test_motor.c`) y archivos de prueba CSV (`workload_test.txt`).

---

## Compilación y Ejecución

### 1. Modo Consola (Windows MinGW-w64)
```powershell
gcc src/*.c -o simulador_so.exe -std=c11 -Wall -Wextra -pthread -lmingw32 -mconsole
.\simulador_so.exe
```

### 2. Modo Consola (Linux / WSL / macOS)
```bash
gcc src/*.c -o simulador_so -std=c11 -Wall -Wextra -pthread
./simulador_so
```
*(También puedes usar `make` directamente en cualquier plataforma).*

### 3. Modo con Interfaz Gráfica (Bonus con Raylib)
```bash
make -f gui/Makefile
./gui/simulador_gui
```

---

## Comandos Disponibles en `SO-Shell>`

| Comando | Descripción |
| :--- | :--- |
| `create <nombre> <burst> [prio]` | Crea un proceso en `READY` (Prioridad opcional: 1 a 5, defecto 3). |
| `load <archivo>` | Carga procesos por lotes desde un archivo CSV respetando `ARRIVAL_TIME`. |
| `ps` | Muestra la tabla de estado actual de todos los procesos. |
| `pcb <PID>` | Muestra el detalle extendido del PCB del proceso indicado. |
| `set_sched <RR\|SRTF> [q]` | Configura algoritmo (`RR` con quantum opcional o `SRTF`), incluso en caliente. |
| `set_speed <ms>` | Configura duración de cada tick de CPU entre 50 y 5000 ms (defecto 1000 ms). |
| `start` | Inicia o reanuda la ejecución del reloj y planificador. |
| `pause` | Congela el reloj de la CPU para inspección estática. |
| `gantt` | Imprime el diagrama de Gantt acumulado hasta el tick actual. |
| `stats` | Muestra promedios de espera, retorno, respuesta y uso de CPU. |
| `help` | Despliega el manual de comandos. |
| `exit` | Detiene ambos hilos limpiamente y libera recursos. |

---

## Escenarios Oficiales de Prueba

### Escenario 1 (Interactivo: RR con cambio en caliente a SRTF)
```text
set_sched RR 2
set_speed 1000
create P1 5 2
create P2 3 1
create P3 2 3
start
ps
pause
pcb 1
set_sched SRTF
start
gantt
stats
```

### Escenario 2 (Carga por Lotes `workload_test.txt` con expropiación SRTF)
```text
set_sched SRTF
load workload_test.txt
start
gantt
stats
```
Resultado esperado de `gantt`:
`[0-1s: Job_A] -> [1-3s: Job_B] -> [3-4s: Job_C] -> [4-7s: Job_A]`

### Pruebas Automatizadas del Motor
```bash
gcc tests/test_motor.c -o test_motor.exe -std=c11 -Wall -Wextra -pthread
.\test_motor.exe
```

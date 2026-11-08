#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
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
    
    errno = 0;
    long val = strtol(str, &endptr, 10);

    if (errno == ERANGE) {
        return false;
    }

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


// Obtiene el siguiente PID disponible en la tabla de procesos.
static int siguiente_pid_libre(void) {
    int mayor_pid = 0;

    for (int i = 0; i < num_procesos; i++) {
        if (procesos[i].pid > mayor_pid) {
            mayor_pid = procesos[i].pid;
        }
    }

    if (mayor_pid == INT_MAX) {
        return 0;
    }

    return mayor_pid + 1;
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

    
    int nuevo_pid = siguiente_pid_libre();

    if (nuevo_pid == 0) {
        printf("[ERROR DEL SISTEMA]: No hay PID disponibles.\n");
        pthread_mutex_unlock(&mutex_sistema);
        return;
    }

    int idx = num_procesos;
    procesos[idx].pid = nuevo_pid;
    strncpy(procesos[idx].nombre, nombre, MAX_NOMBRE_LEN - 1);
    procesos[idx].nombre[MAX_NOMBRE_LEN - 1] = '\0';
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
    motor_encolar_listo(idx); // Asigna estado = LISTO y orden_listo secuencial para desempate FIFO

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
 // COMANDO DE CARGA POR LOTES (JUAN)
 // ============================================================================

#define MAX_LINEA_LOTE 512
#define CAMPOS_LOTE 4

// Datos temporales de un proceso antes de incorporarlo al sistema.
typedef struct {
    char nombre[MAX_NOMBRE_LEN];
    int burst;
    int prioridad;
    int llegada;
} RegistroLote;

// Identificadores de las columnas del archivo.
enum {
    COL_NOMBRE,
    COL_BURST,
    COL_PRIORIDAD,
    COL_LLEGADA
};

// Identifica los encabezados admitidos por el parser.
static int columna_lote(const char* texto) {
    char etiqueta[64];
    size_t len = strlen(texto);

    if (len >= sizeof(etiqueta)) {
        return -1;
    }

    for (size_t i = 0; i < len; i++) {
        etiqueta[i] = (char)tolower((unsigned char)texto[i]);
    }
    etiqueta[len] = '\0';

    if (strcmp(etiqueta, "nombre") == 0 ||
        strcmp(etiqueta, "name") == 0 ||
        strcmp(etiqueta, "proceso") == 0 ||
        strcmp(etiqueta, "process") == 0) {
        return COL_NOMBRE;
    }

    if (strcmp(etiqueta, "burst") == 0 ||
        strcmp(etiqueta, "burst_time") == 0 ||
        strcmp(etiqueta, "rafaga") == 0) {
        return COL_BURST;
    }

    if (strcmp(etiqueta, "prioridad") == 0 ||
        strcmp(etiqueta, "priority") == 0) {
        return COL_PRIORIDAD;
    }

    if (strcmp(etiqueta, "tiempo_llegada") == 0 ||
        strcmp(etiqueta, "arrival") == 0 ||
        strcmp(etiqueta, "arrival_time") == 0 ||
        strcmp(etiqueta, "llegada") == 0) {
        return COL_LLEGADA;
    }

    return -1;
}

// Elimina espacios al principio y al final de un texto.
static char* quitar_espacios(char* str) {
    while (isspace((unsigned char)*str)) {
        str++;
    }

    char* fin = str + strlen(str);

    while (fin > str &&
           isspace((unsigned char)fin[-1])) {
        *--fin = '\0';
    }

    return str;
}

// Divide una línea CSV en cuatro campos obligatorios.
// Se conservan los campos vacíos para detectar errores.
static bool separar_csv_lote(
    char* linea,
    char* campos[CAMPOS_LOTE]
) {
    char* cursor = linea;

    for (int i = 0; i < CAMPOS_LOTE; i++) {
        campos[i] = cursor;
        char* coma = strchr(cursor, ',');

        if (i < CAMPOS_LOTE - 1) {
            if (coma == NULL) {
                return false;
            }

            *coma = '\0';
            cursor = coma + 1;
        } else if (coma != NULL) {
            return false;
        }

        campos[i] = quitar_espacios(campos[i]);

        if (*campos[i] == '\0') {
            return false;
        }
    }

    return true;
}

// Valida los campos y construye un registro temporal.
static bool interpretar_registro_lote(
    char* campos[CAMPOS_LOTE],
    const int posiciones[CAMPOS_LOTE],
    RegistroLote* registro
) {
    const char* nombre = campos[posiciones[COL_NOMBRE]];
    size_t longitud = strlen(nombre);

    // Validar nombre del proceso.
    if (longitud == 0 || longitud >= MAX_NOMBRE_LEN) {
        return false;
    }

    for (size_t i = 0; i < longitud; i++) {
        if (isspace((unsigned char)nombre[i])) {
            return false;
        }
    }

    // Validar duración de CPU.
    if (!parse_int(
            campos[posiciones[COL_BURST]],
            &registro->burst) ||
        registro->burst <= 0) {
        return false;
    }

    // Validar prioridad entre 1 y 5.
    if (!parse_int(
            campos[posiciones[COL_PRIORIDAD]],
            &registro->prioridad) ||
        registro->prioridad < 1 ||
        registro->prioridad > 5) {
        return false;
    }

    // Validar tiempo de llegada no negativo.
    if (!parse_int(
            campos[posiciones[COL_LLEGADA]],
            &registro->llegada) ||
        registro->llegada < 0) {
        return false;
    }

    strcpy(registro->nombre, nombre);

    return true;
}

/*
 * Comando: load <archivo>
 *
 * Lee un archivo CSV y registra procesos masivamente.
 * Primero valida todo el lote para evitar cargas parciales.
 * Los procesos se registran como NUEVO y el motor los
 * admite cuando alcanza su tiempo de llegada.
 */
static void ejecutar_load(int token_count, char* tokens[]) {

    // 1. Validar la sintaxis del comando.
    if (token_count != 2) {
        printf("[ERROR DE SINTAXIS]: Uso correcto: load <archivo.txt>\n");
        return;
    }

    // 2. Abrir el archivo indicado.
    FILE* archivo = fopen(tokens[1], "r");

    if (archivo == NULL) {
        printf("[ERROR DE ARCHIVO]: No se pudo abrir '%s'.\n",
               tokens[1]);
        return;
    }

    // 3. Preparar almacenamiento temporal.
    RegistroLote lote[MAX_PROCESOS];
    int total = 0;
    int linea_num = 0;

    // Orden predeterminado de las columnas.
    int posiciones[CAMPOS_LOTE] = {0, 1, 2, 3};

    bool primera_fila = true;
    bool valido = true;
    char linea[MAX_LINEA_LOTE];

    // 4. Leer el archivo linea por linea.
    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        linea_num++;

        size_t longitud = strlen(linea);

        // Detectar lineas demasiado largas.
        if (longitud > 0 &&
            linea[longitud - 1] != '\n' &&
            !feof(archivo)) {

            printf("[ERROR DE FORMATO]: Linea %d demasiado larga.\n",
                   linea_num);

            valido = false;
            break;
        }

        char* texto = linea;

        // Ignorar BOM UTF-8 si existe.
        if (linea_num == 1 && longitud >= 3 &&
            (unsigned char)linea[0] == 0xEF &&
            (unsigned char)linea[1] == 0xBB &&
            (unsigned char)linea[2] == 0xBF) {

            texto += 3;
        }

        texto = quitar_espacios(texto);

        // Ignorar lineas vacias y comentarios.
        if (*texto == '\0' || *texto == '#') {
            continue;
        }

        // 5. Separar los campos del CSV.
        char* campos[CAMPOS_LOTE];

        if (!separar_csv_lote(texto, campos)) {
            printf("[ERROR DE FORMATO]: Linea %d. "
                   "Se requieren cuatro campos CSV.\n",
                   linea_num);

            valido = false;
            break;
        }

        // 6. Reconocer un posible encabezado.
        if (primera_fila) {
            primera_fila = false;

            int reconocidas = 0;
            int nuevas_posiciones[CAMPOS_LOTE] = {
                -1, -1, -1, -1
            };

            for (int i = 0; i < CAMPOS_LOTE; i++) {
                int col = columna_lote(campos[i]);

                if (col >= 0) {
                    reconocidas++;

                    if (nuevas_posiciones[col] != -1) {
                        printf("[ERROR DE FORMATO]: "
                               "Columnas repetidas en linea %d.\n",
                               linea_num);

                        valido = false;
                        break;
                    }

                    nuevas_posiciones[col] = i;
                }
            }

            if (!valido) {
                break;
            }

            if (reconocidas >= 2) {
                if (reconocidas != CAMPOS_LOTE) {
                    printf("[ERROR DE FORMATO]: "
                           "Encabezado incompleto en linea %d.\n",
                           linea_num);

                    valido = false;
                    break;
                }

                memcpy(posiciones, nuevas_posiciones,
                       sizeof(posiciones));

                continue;
            }
        }

        // 7. Verificar capacidad del lote.
        if (total >= MAX_PROCESOS) {
            printf("[ERROR]: El lote supera el limite "
                   "de %d procesos.\n", MAX_PROCESOS);

            valido = false;
            break;
        }

        // 8. Validar y guardar el registro temporal.
        if (!interpretar_registro_lote(
                campos, posiciones, &lote[total])) {

            printf("[ERROR DE VALIDACION]: Linea %d. "
                   "Verifique nombre, burst, "
                   "prioridad y tiempo de llegada.\n",
                   linea_num);

            valido = false;
            break;
        }

        total++;
    }

    // 9. Comprobar errores de lectura.
    if (ferror(archivo)) {
        printf("[ERROR DE ARCHIVO]: Fallo la lectura de '%s'.\n",
               tokens[1]);

        valido = false;
    }

    fclose(archivo);

    // No insertar procesos si el lote tiene errores.
    if (!valido) {
        printf("[CARGA CANCELADA]: No se agregaron procesos.\n");
        return;
    }

    if (total == 0) {
        printf("[ERROR]: El archivo no contiene procesos.\n");
        return;
    }

    // 10. Ordenar por tiempo de llegada.
    // El orden original se conserva cuando hay empates.
    for (int i = 1; i < total; i++) {
        RegistroLote actual_lote = lote[i];
        int j = i;

        while (j > 0 &&
               lote[j - 1].llegada > actual_lote.llegada) {

            lote[j] = lote[j - 1];
            j--;
        }

        lote[j] = actual_lote;
    }

    // 11. Bloquear la tabla global de procesos.
    pthread_mutex_lock(&mutex_sistema);

    // Comprobar espacio disponible.
    if (total > MAX_PROCESOS - num_procesos) {
        printf("[ERROR]: Capacidad insuficiente. "
               "Disponibles: %d. Solicitados: %d.\n",
               MAX_PROCESOS - num_procesos, total);

        pthread_mutex_unlock(&mutex_sistema);
        return;
    }

    // Obtener identificadores sin duplicados.
    int pid_inicial = siguiente_pid_libre();

    if (pid_inicial == 0 ||
        pid_inicial > INT_MAX - total + 1) {

        printf("[ERROR]: No hay PID suficientes.\n");

        pthread_mutex_unlock(&mutex_sistema);
        return;
    }

    // 12. Instanciar los PCB del lote.
    for (int i = 0; i < total; i++) {

        PCB* p = &procesos[num_procesos++];

        memset(p, 0, sizeof(*p));

        p->pid = pid_inicial + i;

        strcpy(p->nombre, lote[i].nombre);

        p->estado = NUEVO;

        p->burst_inicial = lote[i].burst;
        p->burst_restante = lote[i].burst;

        p->prioridad = lote[i].prioridad;
        p->tiempo_llegada = lote[i].llegada;

        p->tiempo_espera = 0;
        p->tiempo_retorno = -1;
        p->tiempo_respuesta = -1;

        p->tiempo_inicio = -1;
        p->tiempo_finalizacion = -1;

        p->ha_iniciado = false;
        p->orden_listo = 0;

        /*
         * El motor existente admite los procesos
         * NUEVO cuando clock_ticks alcanza su
         * tiempo_llegada.
         */
    }

    pthread_mutex_unlock(&mutex_sistema);

    // 13. Confirmar la carga exitosa.
    printf("[OK]: Se cargaron %d procesos desde '%s'.\n",
           total, tokens[1]);

    printf("[INFO]: PID asignados: %d al %d.\n",
           pid_inicial, pid_inicial + total - 1);
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

        // 1. Comandos de Luis (40%)
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
        // 2. Comandos del Motor de CPU (Renzo - 40%)
        else if (comando_motor(token_count, tokens)) {
            // Atendido por el motor: set_sched, set_speed, start, pause, gantt, stats
        }
        // 3. Comandos de Carga por Lotes (Juan - 20%)
        else if (strcmp(cmd, "load") == 0) {
            ejecutar_load(token_count, tokens);
        } else {
            // Validación estricta ante comandos desconocidos
            printf("[ERROR DE SINTAXIS]: Comando '%s' no reconocido. Escribe 'help' para ver comandos válidos.\n", cmd);
        }
    }

    return NULL;
}

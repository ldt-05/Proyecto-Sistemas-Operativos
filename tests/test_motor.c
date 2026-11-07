// Pruebas del motor con hilos reales. Incluye scheduler.c para ver su estado interno.
// Compilar: gcc tests/test_motor.c -o test_motor.exe -Wall -Wextra -pthread
#include "../src/scheduler.c"

PCB procesos[100];
int num_procesos = 0;
bool simulador_activo = true;
pthread_mutex_t mutex_sistema;

static int fallos = 0;

#define VERIFICAR(cond) do { \
        if (!(cond)) { printf("  FALLO (linea %d): %s\n", __LINE__, #cond); fallos++; } \
    } while (0)

// Lo que haran create y load: llenar el PCB y dejarlo en NUEVO.
static void agregar(const char* nombre, int burst, int prioridad, int llegada) {
    pthread_mutex_lock(&mutex_sistema);
    PCB* p = &procesos[num_procesos];
    memset(p, 0, sizeof(*p));
    p->pid = num_procesos + 1;
    strncpy(p->nombre, nombre, sizeof(p->nombre) - 1);
    p->estado = NUEVO;
    p->burst_inicial = burst;
    p->burst_restante = burst;
    p->prioridad = prioridad;
    p->tiempo_llegada = llegada;
    num_procesos++;
    pthread_mutex_unlock(&mutex_sistema);
}

// Igual que la CLI: strtok separa el comando y comando_motor lee el resto.
static bool cmd(const char* linea) {
    char buf[256];
    strcpy(buf, linea);
    printf("SO-Shell> %s\n", linea);
    return comando_motor(strtok(buf, " "));
}

static void reiniciar(void) {
    pthread_mutex_lock(&mutex_sistema);
    num_procesos = 0;
    clock_ticks = 0;
    actual = SIN_PROCESO;
    quantum_usado = 0;
    ticks_ocupados = 0;
    ultimo_orden = 0;
    gantt_len = 0;
    motor_corriendo = false;
    algoritmo = ALGORITMO_RR;
    quantum = 2;
    velocidad_ms = 50;
    pthread_mutex_unlock(&mutex_sistema);
}

static bool todos_finalizados(void) {
    bool fin = true;
    pthread_mutex_lock(&mutex_sistema);
    for (int i = 0; i < num_procesos; i++) {
        if (procesos[i].estado != FINALIZADO) fin = false;
    }
    pthread_mutex_unlock(&mutex_sistema);
    return fin;
}

static void esperar_fin(void) {
    while (!todos_finalizados()) usleep(2000);
}

static int leer_reloj(void) {
    pthread_mutex_lock(&mutex_sistema);
    int t = clock_ticks;
    pthread_mutex_unlock(&mutex_sistema);
    return t;
}

static void verificar_gantt(const SegmentoGantt* esperado, int n) {
    VERIFICAR(gantt_len == n);
    for (int i = 0; i < n && i < gantt_len; i++) {
        VERIFICAR(gantt[i].indice == esperado[i].indice);
        VERIFICAR(gantt[i].inicio == esperado[i].inicio);
        VERIFICAR(gantt[i].fin == esperado[i].fin);
    }
}

static void verificar_metricas(int i, int retorno, int espera, int respuesta) {
    VERIFICAR(procesos[i].burst_restante == 0);
    VERIFICAR(procesos[i].tiempo_retorno == retorno);
    VERIFICAR(procesos[i].tiempo_espera == espera);
    VERIFICAR(procesos[i].tiempo_respuesta == respuesta);
}

static void prueba_errores(void) {
    printf("\n== Entradas invalidas: nada debe cambiar\n");
    reiniciar();
    const char* malos[] = {
        "set_sched", "set_sched FIFO", "set_sched rr", "set_sched RR 0", "set_sched RR -1",
        "set_sched RR abc", "set_sched RR 2x", "set_sched RR 2 3", "set_sched SRTF 2",
        "set_sched RR 99999999999999999999",
        "set_speed", "set_speed abc", "set_speed 49", "set_speed 5001", "set_speed -100",
        "set_speed 100 200", "set_speed 1.5",
        "start x", "pause x", "gantt x", "stats x",
    };
    for (size_t i = 0; i < sizeof(malos) / sizeof(malos[0]); i++) {
        VERIFICAR(cmd(malos[i]));
    }
    VERIFICAR(algoritmo == ALGORITMO_RR);
    VERIFICAR(quantum == 2);
    VERIFICAR(velocidad_ms == 50);
    VERIFICAR(!motor_corriendo);
    VERIFICAR(!comando_motor("otro"));

    cmd("set_sched RR");
    VERIFICAR(quantum == 2);
    cmd("set_sched RR 7");
    VERIFICAR(quantum == 7);
    cmd("set_speed 5000");
    VERIFICAR(velocidad_ms == 5000);
    cmd("gantt");
    cmd("stats");
    cmd("pause");
    cmd("start");
    cmd("start");
    cmd("pause");
}

static void prueba_rr_completo(void) {
    printf("\n== RR quantum 2 (escenario 1, sin cambio en caliente)\n");
    reiniciar();
    cmd("set_sched RR 2");
    agregar("P1", 5, 2, 0);
    agregar("P2", 3, 1, 0);
    agregar("P3", 2, 3, 0);
    cmd("start");
    esperar_fin();
    cmd("gantt");
    cmd("stats");
    SegmentoGantt g[] = {{0,0,2},{1,2,4},{2,4,6},{0,6,8},{1,8,9},{0,9,10}};
    verificar_gantt(g, 6);
    verificar_metricas(0, 10, 5, 0);
    verificar_metricas(1, 9, 6, 2);
    verificar_metricas(2, 6, 4, 4);
}

static void prueba_cambio_en_caliente(void) {
    printf("\n== Escenario 1: pause, set_sched SRTF y start\n");
    reiniciar();
    cmd("set_sched RR 2");
    agregar("P1", 5, 2, 0);
    agregar("P2", 3, 1, 0);
    agregar("P3", 2, 3, 0);
    cmd("start");
    while (leer_reloj() < 3) usleep(1000);
    cmd("pause");
    int congelado = leer_reloj();
    usleep(300000);  // 6 ticks de tiempo real: el reloj no debe moverse
    VERIFICAR(leer_reloj() == congelado);
    VERIFICAR(congelado == 3);
    cmd("set_sched SRTF");
    cmd("start");
    esperar_fin();
    cmd("gantt");
    cmd("stats");
    SegmentoGantt g[] = {{0,0,2},{1,2,5},{2,5,7},{0,7,10}};
    verificar_gantt(g, 4);
}

static void prueba_srtf_lote(void) {
    printf("\n== Escenario 2: SRTF por lotes\n");
    reiniciar();
    cmd("set_sched SRTF");
    agregar("Job_A", 4, 2, 0);
    agregar("Job_B", 2, 1, 1);
    agregar("Job_C", 1, 3, 3);
    cmd("start");
    esperar_fin();
    usleep(200000);  // Sin trabajo el reloj no debe seguir contando
    cmd("gantt");
    cmd("stats");
    SegmentoGantt g[] = {{0,0,1},{1,1,3},{2,3,4},{0,4,7}};
    verificar_gantt(g, 4);
    VERIFICAR(clock_ticks == 7);
    verificar_metricas(0, 7, 3, 0);
    verificar_metricas(1, 2, 0, 0);
    verificar_metricas(2, 1, 0, 0);
}

static void prueba_idle(void) {
    printf("\n== CPU ociosa antes de una llegada y create con el reloj detenido\n");
    reiniciar();
    agregar("X", 1, 3, 2);
    cmd("start");
    esperar_fin();
    usleep(100000);
    VERIFICAR(clock_ticks == 3);
    agregar("Y", 1, 3, clock_ticks);
    esperar_fin();
    cmd("gantt");
    cmd("stats");
    SegmentoGantt g[] = {{GANTT_IDLE,0,2},{0,2,3},{1,3,4}};
    verificar_gantt(g, 3);
    verificar_metricas(1, 1, 0, 0);
    VERIFICAR(ticks_ocupados == 2);
}

// Un proceso creado a mitad de un tick debe cumplir WT == TAT - burst.
static void prueba_create_a_mitad_de_tick(void) {
    printf("\n== create durante la ejecucion\n");
    reiniciar();
    agregar("A", 3, 3, 0);
    cmd("start");
    while (leer_reloj() < 1) usleep(1000);
    usleep(20000);
    agregar("B", 2, 3, leer_reloj());
    esperar_fin();
    PCB* b = &procesos[1];
    VERIFICAR(b->tiempo_espera == b->tiempo_retorno - b->burst_inicial);
}

static void prueba_tiempo_real(void) {
    printf("\n== Precision del retardo: 20 ticks a 50 ms\n");
    reiniciar();
    agregar("T", 20, 3, 0);
    long long inicio = ahora_ms();
    cmd("start");
    esperar_fin();
    long long total = ahora_ms() - inicio;
    printf("  Duracion real: %lld ms (ideal 1000)\n", total);
    VERIFICAR(total >= 950 && total <= 1400);
}

// start, pause y consultas sin parar mientras el motor corre: el resultado no debe cambiar.
static void prueba_estres(void) {
    printf("\n== Estres: CLI golpeando al motor\n");
    reiniciar();
    cmd("set_sched SRTF");
    agregar("Job_A", 4, 2, 0);
    agregar("Job_B", 2, 1, 1);
    agregar("Job_C", 1, 3, 3);
    cmd("start");
    int vueltas = 0;
    while (!todos_finalizados()) {
        cmd("pause");
        cmd("start");
        cmd("set_speed 50");
        usleep(1000);
        vueltas++;
    }
    printf("  Vueltas: %d\n", vueltas);
    SegmentoGantt g[] = {{0,0,1},{1,1,3},{2,3,4},{0,4,7}};
    verificar_gantt(g, 4);
}

static void prueba_exit_inmediato(void) {
    printf("\n== exit con un tick largo en curso\n");
    reiniciar();
    cmd("set_speed 5000");
    agregar("Largo", 100, 3, 0);
    cmd("start");
    usleep(50000);
    long long inicio = ahora_ms();
    pthread_mutex_lock(&mutex_sistema);
    simulador_activo = false;
    pthread_mutex_unlock(&mutex_sistema);
    motor_finalizar();
    VERIFICAR(ahora_ms() - inicio < 500);
    VERIFICAR(gantt == NULL);
}

int main(void) {
    pthread_mutex_init(&mutex_sistema, NULL);
    motor_iniciar();

    prueba_errores();
    prueba_rr_completo();
    prueba_cambio_en_caliente();
    prueba_srtf_lote();
    prueba_idle();
    prueba_create_a_mitad_de_tick();
    prueba_tiempo_real();
    prueba_estres();
    prueba_exit_inmediato();

    pthread_mutex_destroy(&mutex_sistema);
    printf("\n%s (%d fallos)\n", fallos ? "HAY FALLOS" : "TODO OK", fallos);
    return fallos != 0;
}

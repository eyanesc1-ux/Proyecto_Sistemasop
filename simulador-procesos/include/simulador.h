/*
 * simulador.h — Simulador del Subsistema de Procesos, Algoritmos de Planificación y Kernel
 * Proyecto Final Sistemas Operativos — Fase 2
 *
 * Funciones del programa:
 *   - Realiza una prueba de estrés con una carga masiva de procesos concurrentes (>= 100).
 *     Hace una comparativa de los siguientes algoritmos de planfificación: FCFS, Round Robin, SJF, SRT y Realimentación Multinivel
 *     sobre la misma carga, midiendo Turnaround, Waiting Time y Throughput. Analiza el barrido del quantum de Round Robin.
 *   - Genera una bitácora de eventos del kernel por algoritmo.
 *   - Realiza gráficas comparativas en SVG y un reporte HTML.
 */
#ifndef SIMULADOR_H
#define SIMULADOR_H

#include <stdint.h>
#include <stdio.h>

/* Bloque de control de procesos (PCB) y modelo de 5 estados. */
typedef enum {
    NUEVO = 0,   /* porceso creado pero aún no admitido. */
    LISTO,       /* proceso en la cola de listos esperando CPU. */
    EJECUCION,   /* ocupando la CPU. */
    BLOQUEADO,   /* esperando una operación de E/S */
    TERMINADO,   /* finalizó y el PCB se conserva para el calculo de las métricas    */
    N_ESTADOS
} estado_t;

typedef struct pcb {
    /* Realiza la identificación y datos de la carga */
    int pid;
    int llegada;        /* instante de creación (tick) */
    int rafaga;         /* CPU total que necesita */
    int io_cada;        /* pide E/S, cada io_cada ticks de CPU (0 = no)*/
    int io_duracion;    /* duración de cada E/S */

    /* Contexto que el kernel guarda y restaura en cada cambio */
    estado_t estado;
    int restante;       /* CPU que le falta */
    int cpu_desde_io;   /* CPU consumida desde la última E/S */
    int io_restante;    /* ticks que le faltan en BLOQUEADO  */
    int nivel;          /* nivel en la cola multinivel (MLFQ) */
    int quantum_usado;  /* ticks usados en el despacho actual */

    /* Contabilidad para realizar las métricas */
    int t_primera_cpu;  /* primer despacho (-1 = nunca)               */
    int t_fin;          /* instante de terminación (-1 = no terminó)  */
    int t_espera;       /* ticks acumulados en LISTO (Waiting Time)   */
    int t_bloqueado;    /* ticks acumulados en E/S */

    /* Enlaces de la cola de listos (uso de lista doblemente enlazada) */
    struct pcb *sig, *ant;
} pcb_t;

/* Cola de listos (FIFO apropiativo para los algoritmos SJF/SRT). */
typedef struct {
    pcb_t *cabeza, *cola;
    int tam;
} cola_t;

/* Algoritmos, configuración y métricas                                */
typedef enum { ALG_FCFS = 0, ALG_RR, ALG_SJF, ALG_SRT, ALG_MLFQ, N_ALG } algoritmo_t;

#define MLFQ_NIVELES 3

typedef struct {
    algoritmo_t alg;
    int   quantum;                     /* Round Robin */
    int   cambio_contexto;             /* costo en ticks de cada cambio */
    int   mlfq_quantum[MLFQ_NIVELES];  /* quantum por nivel (0 = FCFS) */
    int   mlfq_boost;                  /* cada cuántos ticks suben todos */
    FILE *bitacora;
} config_t;

typedef struct {
    int    n, finalizados, tiempo_total;
    long   ticks_cpu, ticks_cc, ticks_ocioso;
    long   cambios_contexto, expropiaciones;
    long   transiciones[N_ESTADOS][N_ESTADOS];
    long   transiciones_ilegales;      /* debe ser 0  */
    double tat_prom, wt_prom, rt_prom; /* Turnaround, Waiting, Respuesta   */
    int    tat_max, wt_max;
    double throughput;                 /* procesos / 1000 ticks  */
    double utilizacion;                /* % CPU ejecutando procesos */
    double sobrecarga;                 /* % CPU en cambios de contexto */
    double ms_reales;                  /* tiempo real que tardó la corrida */
} metricas_t;

/* kernel.c */
const char *estado_nombre(estado_t e);
const char *algoritmo_nombre(algoritmo_t a);
int         algoritmo_de_texto(const char *s);         /* -1 si no existe */
void        config_defecto(config_t *c);
void        pcb_init(pcb_t *p, int pid, int llegada, int rafaga,
                     int io_cada, int io_duracion);
void        simular(pcb_t *procs, int n, const config_t *cfg, metricas_t *m);

/* carga.c */
void carga_generar(pcb_t *procs, int n, uint32_t semilla, int ventana,
                   int pct_io);
void carga_copiar(const pcb_t *origen, pcb_t *destino, int n);
int  carga_guardar_csv(const char *ruta, const pcb_t *procs, int n);

/* graficas.c */
typedef struct {
    const char *etiqueta;  /* "FCFS", "RR q=4", ...               */
    algoritmo_t alg;
    metricas_t  m;
} fila_comparativa_t;

typedef struct {
    int        quantum;
    metricas_t m;
} fila_barrido_t;

void graficas_generar(const char *dir, const fila_comparativa_t *comp, int n_comp, const fila_barrido_t *barr, int n_barr, const metricas_t *fcfs, int procesos, int cc, int max_rafaga, const char *equipo);

#endif

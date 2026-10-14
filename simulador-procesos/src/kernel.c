/*
 * kernel.c — Núcleo del simulador: PCB, cola de listos, transiciones de estado, despachador, cambio de contexto y algoritmos de planificación
 * Tiempo discreto: 1 tick = 1 unidad de tiempo (por ejemplo 1 ms). En cada tick:
 *   1. "timer tick" de MLFQ (boost periódico),
 *   2. ¿hay que expropiar al proceso en CPU? (SRT, MLFQ),
 *   3. si la CPU está libre: planificador + despachador (+ cambio de contexto),
 *   4. avanza un tick (CPU, E/S y espera en la cola de listos),
 *   5. interrupciones de fin de tick: llegadas, fin de E/S, fin de proceso,
 *      solicitud de E/S y expiración del quantum.
 */
#include "simulador.h"

#include <stdlib.h>
#include <string.h>

/* Nombres                                                             */
const char *estado_nombre(estado_t e)
{
    static const char *n[N_ESTADOS] = {"NUEVO", "LISTO", "EJECUCION",
                                       "BLOQUEADO", "TERMINADO"};
    return (e >= 0 && e < N_ESTADOS) ? n[e] : "?";
}

const char *algoritmo_nombre(algoritmo_t a)
{
    static const char *n[N_ALG] = {"FCFS", "RR", "SJF", "SRT", "MLFQ"};
    return (a >= 0 && a < N_ALG) ? n[a] : "?";
}

int algoritmo_de_texto(const char *s)
{
    for (int i = 0; i < N_ALG; i++) {
        const char *a = algoritmo_nombre(i), *b = s;
        while (*a && *b && (*a | 32) == (*b | 32)) { a++; b++; }
        if (!*a && !*b)
            return i;
    }
    return -1;
}

void config_defecto(config_t *c)
{
    memset(c, 0, sizeof *c);
    c->alg = ALG_RR;
    c->quantum = 4;
    c->cambio_contexto = 1;
    c->mlfq_quantum[0] = 8;    /* nivel 0: RR q=8   (interactivos)   */
    c->mlfq_quantum[1] = 16;   /* nivel 1: RR q=16                   */
    c->mlfq_quantum[2] = 0;    /* nivel 2: FCFS     (intensivos CPU) */
    c->mlfq_boost = 500;
}

void pcb_init(pcb_t *p, int pid, int llegada, int rafaga, int io_cada,
              int io_duracion)
{
    memset(p, 0, sizeof *p);
    p->pid = pid;
    p->llegada = llegada;
    p->rafaga = p->restante = rafaga;
    p->io_cada = (io_cada > 0 && io_duracion > 0) ? io_cada : 0;
    p->io_duracion = p->io_cada ? io_duracion : 0;
    p->estado = NUEVO;
    p->t_primera_cpu = p->t_fin = -1;
}

/* Próxima ráfaga de CPU: hasta terminar o hasta su siguiente E/S. */
static int proxima_rafaga(const pcb_t *p)
{
    if (p->io_cada > 0) {
        int hasta_io = p->io_cada - p->cpu_desde_io;
        return hasta_io < p->restante ? hasta_io : p->restante;
    }
    return p->restante;
}

/* Cola de listos                                                      */
static void encolar(cola_t *c, pcb_t *p)
{
    p->sig = NULL;
    p->ant = c->cola;
    if (c->cola) c->cola->sig = p; else c->cabeza = p;
    c->cola = p;
    c->tam++;
}

static void remover(cola_t *c, pcb_t *p)
{
    if (p->ant) p->ant->sig = p->sig; else c->cabeza = p->sig;
    if (p->sig) p->sig->ant = p->ant; else c->cola = p->ant;
    p->sig = p->ant = NULL;
    c->tam--;
}

/* Menor próxima ráfaga; empate: llegó antes, luego menor PID. */
static pcb_t *buscar_mas_corto(const cola_t *c)
{
    pcb_t *mejor = c->cabeza;
    for (pcb_t *p = mejor ? mejor->sig : NULL; p; p = p->sig) {
        int d = proxima_rafaga(p) - proxima_rafaga(mejor);
        if (d < 0 || (d == 0 && (p->llegada < mejor->llegada ||
                                 (p->llegada == mejor->llegada && p->pid < mejor->pid))))
            mejor = p;
    }
    return mejor;
}

/* Estado interno del kernel durante una simulación */

typedef struct {
    config_t   cfg;
    pcb_t     *procs;
    int        n;
    cola_t     listos[MLFQ_NIVELES];  /* RR/FCFS/SJF/SRT usan sólo [0] */
    pcb_t     *ejecutando;            /* proceso en CPU                 */
    pcb_t     *entrante;              /* contexto que se está cargando  */
    pcb_t     *ultimo;                /* último que tuvo la CPU         */
    int        cc_restante;
    int        reloj;
    metricas_t m;
} kernel_t;

static void bitacora(kernel_t *k, const char *evento, const pcb_t *p,
                     const char *detalle)
{
    if (k->cfg.bitacora)
        fprintf(k->cfg.bitacora, "[t=%6d] %-10s PID %4d  %s\n", k->reloj,
                evento, p ? p->pid : -1, detalle);
}

/*
 * Única puerta para cambiar el estado de un PCB. Valida la transición
 * contra el modelo de 5 estados, la cuenta y la registra en la bitácora.
 */
static void transicion(kernel_t *k, pcb_t *p, estado_t nuevo, const char *motivo)
{
    static const int valida[N_ESTADOS][N_ESTADOS] = {
        /*            NUE LIS EJE BLO TER */
        /* NUEVO */  { 0,  1,  0,  0,  0 },   /* admitir            */
        /* LISTO */  { 0,  0,  1,  0,  0 },   /* despachar          */
        /* EJEC. */  { 0,  1,  0,  1,  1 },   /* quantum/E-S/exit   */
        /* BLOQ. */  { 0,  1,  0,  0,  0 },   /* fin de E/S         */
        /* TERM. */  { 0,  0,  0,  0,  0 },
    };
    char det[96];
    estado_t viejo = p->estado;
    if (!valida[viejo][nuevo]) {
        k->m.transiciones_ilegales++;
        snprintf(det, sizeof det, "TRANSICION ILEGAL %s -> %s",
                 estado_nombre(viejo), estado_nombre(nuevo));
        bitacora(k, "ERROR", p, det);
        return;
    }
    p->estado = nuevo;
    k->m.transiciones[viejo][nuevo]++;
    snprintf(det, sizeof det, "%-9s -> %-9s %s", estado_nombre(viejo),
             estado_nombre(nuevo), motivo);
    bitacora(k, "ESTADO", p, det);
}

/* Política de planificación */
/* Pasa un PCB a LISTO y lo inserta donde indica el algoritmo. */
static void a_listos(kernel_t *k, pcb_t *p, const char *motivo)
{
    transicion(k, p, LISTO, motivo);
    encolar(&k->listos[k->cfg.alg == ALG_MLFQ ? p->nivel : 0], p);
}

/* Planificador: se encargara de elegir y sacar de la cola al siguiente proceso. */
static pcb_t *seleccionar(kernel_t *k)
{
    pcb_t *p = NULL;
    switch (k->cfg.alg) {
    case ALG_SJF:
    case ALG_SRT:                              /* el más corto primero  */
        p = buscar_mas_corto(&k->listos[0]);
        if (p) remover(&k->listos[0], p);
        return p;
    case ALG_MLFQ:                             /* primer nivel no vacío */
        for (int i = 0; i < MLFQ_NIVELES; i++)
            if ((p = k->listos[i].cabeza) != NULL) {
                remover(&k->listos[i], p);
                return p;
            }
        return NULL;
    default:                                   /* FCFS y RR: FIFO       */
        p = k->listos[0].cabeza;
        if (p) remover(&k->listos[0], p);
        return p;
    }
}

/* ¿Debe desalojarse al proceso en CPU? */
static int debe_expropiar(kernel_t *k, pcb_t *run)
{
    if (k->cfg.alg == ALG_SRT) {               /* llegó alguien más corto */
        pcb_t *c = buscar_mas_corto(&k->listos[0]);
        return c && proxima_rafaga(c) < proxima_rafaga(run);
    }
    if (k->cfg.alg == ALG_MLFQ) {              /* hay un nivel superior   */
        for (int i = 0; i < run->nivel; i++)
            if (k->listos[i].tam > 0)
                return 1;
    }
    return 0;
}

/* Quantum vigente para el proceso (0 = sin límite). */
static int quantum_de(kernel_t *k, pcb_t *p)
{
    if (k->cfg.alg == ALG_RR)   return k->cfg.quantum;
    if (k->cfg.alg == ALG_MLFQ) return k->cfg.mlfq_quantum[p->nivel];
    return 0;
}

/* MLFQ: cada mlfq_boost ticks todos suben al nivel 0 (evita inanición). */
static void boost_mlfq(kernel_t *k)
{
    if (k->cfg.alg != ALG_MLFQ || k->cfg.mlfq_boost <= 0 || k->reloj == 0 ||
        k->reloj % k->cfg.mlfq_boost != 0)
        return;
    for (int i = 1; i < MLFQ_NIVELES; i++) {
        pcb_t *p;
        while ((p = k->listos[i].cabeza) != NULL) {
            remover(&k->listos[i], p);
            p->nivel = 0;
            encolar(&k->listos[0], p);
        }
    }
    for (int i = 0; i < k->n; i++)
        k->procs[i].nivel = 0;
    bitacora(k, "BOOST", NULL, "todos los procesos suben al nivel 0");
}
/* Despachador                                                         */
static void despachar(kernel_t *k, pcb_t *p)
{
    char det[64];
    transicion(k, p, EJECUCION, "");
    if (p->t_primera_cpu < 0)
        p->t_primera_cpu = k->reloj;
    p->quantum_usado = 0;
    k->ejecutando = k->ultimo = p;
    snprintf(det, sizeof det, "nivel=%d restante=%d", p->nivel, p->restante);
    bitacora(k, "DESPACHO", p, det);
}

static int cmp_llegada(const void *a, const void *b)
{
    const pcb_t *x = *(pcb_t *const *)a, *y = *(pcb_t *const *)b;
    return x->llegada != y->llegada ? x->llegada - y->llegada : x->pid - y->pid;
}

/* Bucle principal  */
void simular(pcb_t *procs, int n, const config_t *cfg, metricas_t *out)
{
    kernel_t k;
    memset(&k, 0, sizeof k);
    k.cfg = *cfg;
    k.procs = procs;
    k.n = n;

    /* Orden de llegada (desempate por PID: resultado determinista). */
    pcb_t **orden = malloc(sizeof(pcb_t *) * (n > 0 ? n : 1));
    for (int i = 0; i < n; i++) orden[i] = &procs[i];
    qsort(orden, n, sizeof(pcb_t *), cmp_llegada);
    int sig_llegada = 0;

    if (k.cfg.bitacora)
        fprintf(k.cfg.bitacora, "# Bitacora del kernel - %s quantum=%d "
                "cambio_contexto=%d procesos=%d\n", algoritmo_nombre(cfg->alg),
                cfg->quantum, cfg->cambio_contexto, n);

    while (sig_llegada < n && orden[sig_llegada]->llegada <= 0)
        a_listos(&k, orden[sig_llegada++], "(admitido)");

    while (k.m.finalizados < n) {
        /* (1) timer tick de MLFQ */
        boost_mlfq(&k);

        /* (2) expropiación */
        if (k.ejecutando && debe_expropiar(&k, k.ejecutando)) {
            pcb_t *p = k.ejecutando;
            k.ejecutando = NULL;
            k.m.expropiaciones++;
            a_listos(&k, p, "(expropiado)");
        }

        /* (3) planificador + despachador */
        if (!k.ejecutando && !k.entrante) {
            pcb_t *p = seleccionar(&k);
            if (p && p != k.ultimo) {
                /* Cambio de contexto: guardar el PCB saliente y cargar el
                   entrante. Mientras dura, la CPU trabaja para el kernel. */
                k.m.cambios_contexto++;
                if (cfg->cambio_contexto > 0) {
                    k.entrante = p;
                    k.cc_restante = cfg->cambio_contexto;
                    bitacora(&k, "CTX-SWITCH", p, "carga de contexto");
                } else {
                    despachar(&k, p);
                }
            } else if (p) {
                despachar(&k, p); /* mismo proceso sin cambio */
            }
        }

        /* (4) avanza un tick */
        if (k.entrante) {
            k.m.ticks_cc++;
        } else if (k.ejecutando) {
            k.ejecutando->restante--;
            k.ejecutando->quantum_usado++;
            k.ejecutando->cpu_desde_io++;
            k.m.ticks_cpu++;
        } else {
            k.m.ticks_ocioso++;
        }
        for (int i = 0; i < n; i++) {
            if (procs[i].estado == LISTO)
                procs[i].t_espera++;           /* Waiting Time */
            else if (procs[i].estado == BLOQUEADO) {
                procs[i].io_restante--;
                procs[i].t_bloqueado++;
            }
        }
        k.reloj++;

        /* (5) interrupciones de fin de tick */
        while (sig_llegada < n && orden[sig_llegada]->llegada <= k.reloj)
            a_listos(&k, orden[sig_llegada++], "(admitido)");

        for (int i = 0; i < n; i++)
            if (procs[i].estado == BLOQUEADO && procs[i].io_restante <= 0)
                a_listos(&k, &procs[i], "(fin de E/S)");

        if (k.entrante) {
            if (--k.cc_restante == 0) {
                pcb_t *p = k.entrante;
                k.entrante = NULL;
                despachar(&k, p);
            }
        } else if (k.ejecutando) {
            pcb_t *p = k.ejecutando;
            int q = quantum_de(&k, p);
            if (p->restante == 0) {
                k.ejecutando = NULL;
                p->t_fin = k.reloj;
                k.m.finalizados++;
                transicion(&k, p, TERMINADO, "(exit)");
            } else if (p->io_cada > 0 && p->cpu_desde_io >= p->io_cada) {
                k.ejecutando = NULL;
                p->cpu_desde_io = 0;
                p->io_restante = p->io_duracion;
                transicion(&k, p, BLOQUEADO, "(solicita E/S)");
            } else if (q > 0 && p->quantum_usado >= q) {
                k.ejecutando = NULL;
                if (cfg->alg == ALG_MLFQ && p->nivel < MLFQ_NIVELES - 1)
                    p->nivel++;                /* MLFQ: baja de nivel */
                a_listos(&k, p, "(fin de quantum)");
            }
        }
    }
    free(orden);

    /* Métricas */
    metricas_t *m = &k.m;
    double s_tat = 0, s_wt = 0, s_rt = 0;
    m->n = n;
    m->tiempo_total = k.reloj;
    for (int i = 0; i < n; i++) {
        int tat = procs[i].t_fin - procs[i].llegada;
        int wt = procs[i].t_espera;
        s_tat += tat;
        s_wt += wt;
        s_rt += procs[i].t_primera_cpu - procs[i].llegada;
        if (tat > m->tat_max) m->tat_max = tat;
        if (wt > m->wt_max) m->wt_max = wt;
    }
    if (n > 0) {
        m->tat_prom = s_tat / n;
        m->wt_prom = s_wt / n;
        m->rt_prom = s_rt / n;
    }
    if (m->tiempo_total > 0) {
        m->throughput = 1000.0 * m->finalizados / m->tiempo_total;
        m->utilizacion = 100.0 * m->ticks_cpu / m->tiempo_total;
        m->sobrecarga = 100.0 * m->ticks_cc / m->tiempo_total;
    }
    *out = k.m;
}

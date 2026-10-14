/*
 * main.c — Programa principal del simulador (Fase 2).
 *
 * Sin argumentos ejecuta todo:
 *   1. genera la carga de estrés (120 procesos concurrentes),
 *   2. comparativa de los 5 algortimos a trabajar FCFS, RR, SJF, SRT y MLFQ con bitácora por algoritmo,
 *   3. barrido del quantum de Round Robin,
 *   4. gráficas SVG + reporte HTML.
 * Todo queda en la carpeta "salida" (o la indicada con -o).
 *
 * glosario breve: simplanif [-n procesos] [-s semilla] [-q quantum] [-c costo_cc]
 *                [-w ventana] [-i pct_io] [-Q lista_quantums] [-o carpeta]
 */
#include "simulador.h"

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define CREAR_DIR(d) _mkdir(d)
#else
#include <sys/stat.h>
#include <sys/utsname.h>
#define CREAR_DIR(d) mkdir((d), 0755)
#endif

#define MAX_Q 64

/* Utilidades                                                          */
static FILE *consola = NULL;   /* copia de todo lo impreso: consola.txt */

/* Imprime en pantalla y en salida/consola.txt al mismo tiempo. */
static void out(const char *fmt, ...)
{
    va_list a, b;
    va_start(a, fmt);
    va_copy(b, a);
    vprintf(fmt, a);
    if (consola)
        vfprintf(consola, fmt, b);
    va_end(b);
    va_end(a);
}

static int crear_dir(const char *d)
{
    if (CREAR_DIR(d) != 0 && errno != EEXIST) {
        perror(d);
        return -1;
    }
    return 0;
}

/* Reloj de pared en milisegundos */
static double reloj_ms(void)
{
#ifdef _WIN32
    LARGE_INTEGER frec, t;
    QueryPerformanceFrequency(&frec);
    QueryPerformanceCounter(&t);
    return t.QuadPart * 1000.0 / frec.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
#endif
}

/* Nombre del equipo y sistema operativo que usa, para diferenciar en que vm se esta probando */
static void identificar_equipo(char *buf, size_t n)
{
#ifdef _WIN32
    const char *host = getenv("COMPUTERNAME");
    const char *arq = getenv("PROCESSOR_ARCHITECTURE");
    snprintf(buf, n, "%s (Windows, %s)", host ? host : "?", arq ? arq : "?");
#else
    struct utsname u;
    if (uname(&u) == 0)
        snprintf(buf, n, "%.60s (%.20s %.40s, %.20s)", u.nodename, u.sysname, u.release,
                 u.machine);
    else
        snprintf(buf, n, "? (POSIX)");
#endif
}

static void uso(void)
{
    printf("Uso: simplanif [opciones]\n"
           "  -n N      procesos de la prueba de estres (def. 120, minimo 100)\n"
           "  -s N      semilla del generador (def. 2026)\n"
           "  -q N      quantum de Round Robin en la comparativa (def. 4)\n"
           "  -c N      costo del cambio de contexto en ticks (def. 1)\n"
           "  -w N      ventana de llegadas: todos llegan en [0, N] (def. 100)\n"
           "  -i N      %% de procesos con E/S (def. 30)\n"
           "  -Q LISTA  quantums del barrido, p. ej. 1,2,4,8,16 (def. 20 valores)\n"
           "  -o DIR    carpeta de salida (def. salida)\n"
           "  -h        esta ayuda\n");
}

/* Programa principal */
int main(int argc, char **argv)
{
    int n = 120, quantum = 4, cc = 1, ventana = 100, pct_io = 30;
    uint32_t semilla = 2026;
    const char *dir = "salida";
    char lista_q[256] = "1,2,3,4,5,6,8,10,12,16,20,24,32,40,48,64,80,100,120,150";

    for (int i = 1; i < argc; i++) {
        const char *o = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(o, "-h") || !strcmp(o, "--help") || !strcmp(o, "/?")) {
            uso();
            return 0;
        }
        if (!v) { uso(); return 2; }
        if      (!strcmp(o, "-n")) n = atoi(v);
        else if (!strcmp(o, "-s")) semilla = (uint32_t)strtoul(v, NULL, 10);
        else if (!strcmp(o, "-q")) quantum = atoi(v);
        else if (!strcmp(o, "-c")) cc = atoi(v);
        else if (!strcmp(o, "-w")) ventana = atoi(v);
        else if (!strcmp(o, "-i")) pct_io = atoi(v);
        else if (!strcmp(o, "-Q")) snprintf(lista_q, sizeof lista_q, "%s", v);
        else if (!strcmp(o, "-o")) dir = v;
        else { uso(); return 2; }
        i++;
    }
    if (n < 100) {
        printf("La prueba de estres requiere al menos 100 procesos (se usara 100).\n");
        n = 100;
    }
    if (quantum < 1 || cc < 0 || ventana < 0) {
        uso();
        return 2;
    }

    /* Carpetas de salida */
    char ruta[512], equipo[256];
    if (crear_dir(dir)) return 1;
    snprintf(ruta, sizeof ruta, "%s/bitacoras", dir);
    if (crear_dir(ruta)) return 1;
    snprintf(ruta, sizeof ruta, "%s/graficas", dir);
    if (crear_dir(ruta)) return 1;
    snprintf(ruta, sizeof ruta, "%s/consola.txt", dir);
    consola = fopen(ruta, "w");

    identificar_equipo(equipo, sizeof equipo);
    time_t ahora = time(NULL);
    char fecha[64];
    strftime(fecha, sizeof fecha, "%Y-%m-%d %H:%M:%S", localtime(&ahora));
    double t_inicio = reloj_ms();

    out("==============================================================\n");
    out(" Simulador de planificacion de CPU - Fase 2\n");
    out(" Equipo: %s\n", equipo);
    out(" Fecha:  %s\n", fecha);
    out("==============================================================\n\n");

    /* 1. Carga de estrés (inyectos de la cantidad n de procesos) */
    pcb_t *base = malloc(sizeof(pcb_t) * n);
    pcb_t *trabajo = malloc(sizeof(pcb_t) * n);
    if (!base || !trabajo) {
        out("Sin memoria\n");
        return 1;
    }
    carga_generar(base, n, semilla, ventana, pct_io);
    long cpu_total = 0;
    int con_io = 0, max_rafaga = 0;
    for (int i = 0; i < n; i++) {
        cpu_total += base[i].rafaga;
        con_io += base[i].io_cada > 0;
        if (base[i].rafaga > max_rafaga) max_rafaga = base[i].rafaga;
    }
    snprintf(ruta, sizeof ruta, "%s/carga.csv", dir);
    carga_guardar_csv(ruta, base, n);
    out("[1] Carga de estres: %d procesos concurrentes (llegan entre t=0 y t=%d)\n",
        n, ventana);
    out("    CPU demandada: %ld ticks | con E/S: %d | rafaga maxima: %d | "
        "semilla: %u\n", cpu_total, con_io, max_rafaga, semilla);
    out("    Cambio de contexto: %d tick(s) | MLFQ: niveles RR q=8, RR q=16, FCFS; "
        "boost cada 500\n\n", cc);

    /*  2. Comparativa  */
    fila_comparativa_t comp[N_ALG];
    char etq_rr[32];
    snprintf(etq_rr, sizeof etq_rr, "RR q=%d", quantum);
    snprintf(ruta, sizeof ruta, "%s/comparativa.csv", dir);
    FILE *csv = fopen(ruta, "w");
    if (csv)
        fprintf(csv, "algoritmo,quantum,procesos,terminados,tiempo_total," "turnaround_prom,waiting_prom,respuesta_prom,turnaround_max," "waiting_max,throughput_por_1000,cpu_util_pct,"
        "cambio_contexto_pct,cambios_contexto,expropiaciones," "transiciones_ilegales,ms_reales\n");

    out("[2] Comparativa de algoritmos (bitacoras en %s/bitacoras)\n\n", dir);
    out("%-8s | %9s %9s %9s %7s | %8s %6s %6s | %6s %6s | %8s\n", "Alg",
        "TAT prom", "WT prom", "Resp", "WT max", "Thr/1kt", "CPU%", "CC%",
        "#CC", "Exprop", "ms real");
    out("---------------------------------------------------------------------"
        "-------------------------\n");
    for (int a = 0; a < N_ALG; a++) {
        config_t cfg;
        config_defecto(&cfg);
        cfg.alg = a;
        cfg.quantum = quantum;
        cfg.cambio_contexto = cc;
        snprintf(ruta, sizeof ruta, "%s/bitacoras/bitacora_%s.log", dir,
                 algoritmo_nombre(a));
        cfg.bitacora = fopen(ruta, "w");

        carga_copiar(base, trabajo, n);
        double t0 = reloj_ms();
        simular(trabajo, n, &cfg, &comp[a].m);
        comp[a].m.ms_reales = reloj_ms() - t0;
        if (cfg.bitacora) fclose(cfg.bitacora);

        comp[a].alg = a;
        comp[a].etiqueta = a == ALG_RR ? etq_rr : algoritmo_nombre(a);
        const metricas_t *m = &comp[a].m;
        out("%-8s | %9.2f %9.2f %9.2f %7d | %8.3f %6.2f %6.2f | %6ld %6ld | %8.1f\n",
            comp[a].etiqueta, m->tat_prom, m->wt_prom, m->rt_prom, m->wt_max,
            m->throughput, m->utilizacion, m->sobrecarga, m->cambios_contexto,
            m->expropiaciones, m->ms_reales);
        if (csv)
            fprintf(csv, "%s,%d,%d,%d,%d,%.3f,%.3f,%.3f,%d,%d,%.4f,%.3f,%.3f,"
                         "%ld,%ld,%ld,%.1f\n", algoritmo_nombre(a),
                    a == ALG_RR ? quantum : 0, m->n, m->finalizados,
                    m->tiempo_total, m->tat_prom, m->wt_prom, m->rt_prom,
                    m->tat_max, m->wt_max, m->throughput, m->utilizacion,
                    m->sobrecarga, m->cambios_contexto, m->expropiaciones,
                    m->transiciones_ilegales, m->ms_reales);
    }
    if (csv) fclose(csv);

    out("\n    Transiciones de estado por algoritmo (administradas por el kernel):\n");
    out("    %-8s %8s %8s %8s %8s %8s %8s %9s\n", "Alg", "N->L", "L->E",
        "E->L", "E->B", "B->L", "E->T", "ilegales");
    for (int a = 0; a < N_ALG; a++) {
        const metricas_t *m = &comp[a].m;
        out("    %-8s %8ld %8ld %8ld %8ld %8ld %8ld %9ld\n", comp[a].etiqueta,
            m->transiciones[NUEVO][LISTO], m->transiciones[LISTO][EJECUCION],
            m->transiciones[EJECUCION][LISTO], m->transiciones[EJECUCION][BLOQUEADO],
            m->transiciones[BLOQUEADO][LISTO], m->transiciones[EJECUCION][TERMINADO],
            m->transiciones_ilegales);
    }
    out("\n");

    /* 3. Barrido del quantum para algortimo RR */
    fila_barrido_t barr[MAX_Q];
    int nb = 0;
    snprintf(ruta, sizeof ruta, "%s/barrido_quantum.csv", dir);
    csv = fopen(ruta, "w");
    if (csv)
        fprintf(csv, "quantum,tiempo_total,turnaround_prom,waiting_prom," "respuesta_prom,throughput_por_1000,cpu_util_pct," "cambio_contexto_pct,cambios_contexto\n");
    out("[3] Barrido del quantum de Round Robin (referencia FCFS: TAT %.2f, "
        "WT %.2f)\n\n", comp[ALG_FCFS].m.tat_prom, comp[ALG_FCFS].m.wt_prom);
    out("%8s | %9s %9s %9s | %8s %6s %6s\n", "Quantum", "TAT prom", "WT prom",
        "Resp", "Thr/1kt", "CC%", "#CC");
    out("------------------------------------------------------------------\n");
    for (char *tok = strtok(lista_q, ","); tok && nb < MAX_Q; tok = strtok(NULL, ",")) {
        int q = atoi(tok);
        if (q < 1) continue;
        config_t cfg;
        config_defecto(&cfg);
        cfg.alg = ALG_RR;
        cfg.quantum = q;
        cfg.cambio_contexto = cc;
        carga_copiar(base, trabajo, n);
        simular(trabajo, n, &cfg, &barr[nb].m);
        barr[nb].quantum = q;
        const metricas_t *m = &barr[nb].m;
        out("%8d | %9.2f %9.2f %9.2f | %8.3f %6.2f %6ld\n", q, m->tat_prom,
            m->wt_prom, m->rt_prom, m->throughput, m->sobrecarga,
            m->cambios_contexto);
        if (csv)
            fprintf(csv, "%d,%d,%.3f,%.3f,%.3f,%.4f,%.3f,%.3f,%ld\n", q,
                    m->tiempo_total, m->tat_prom, m->wt_prom, m->rt_prom,
                    m->throughput, m->utilizacion, m->sobrecarga,
                    m->cambios_contexto);
        nb++;
    }
    if (csv) fclose(csv);
    out("\n    Quantum pequenio -> muchos cambios de contexto (se sobrecarga la CPU).\n");
    out("    Quantum >= rafaga maxima (%d) -> Round Robin se comporta como FCFS.\n\n",
        max_rafaga);

    /* ---------------- 4. Gráficas ---------------- */
    graficas_generar(dir, comp, N_ALG, barr, nb, &comp[ALG_FCFS].m, n, cc,
                     max_rafaga, equipo);
    out("[4] Graficas y reporte generados en %s:\n", dir);
    out("    reporte.html, graficas/g1_comparativa.svg, g2_quantum_tiempos.svg,\n"
        "    g3_transiciones.svg\n\n");

    long ilegales = 0;
    int terminados = 1;
    for (int a = 0; a < N_ALG; a++) {
        ilegales += comp[a].m.transiciones_ilegales;
        terminados &= comp[a].m.finalizados == n;
    }
    out("Resultado: %s | transiciones ilegales: %ld | tiempo total: %.0f ms\n",
        terminados ? "todos los procesos terminaron en los 5 algoritmos"
                   : "HUBO PROCESOS SIN TERMINAR", ilegales, reloj_ms() - t_inicio);

    if (consola) fclose(consola);
    free(base);
    free(trabajo);
    return (terminados && ilegales == 0) ? 0 : 1;
}

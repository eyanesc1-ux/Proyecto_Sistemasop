/*
 * En lugar de usar la función estándar rand(), este sistema utiliza 
 un generador propio (xorshift32). Al usar la misma semilla, 
 el comportamiento es exactamente idéntico tanto en Linux como en Windows, 
 lo que garantiza que los resultados de los diferentes nodos de la red se puedan comparar de forma
 */
#include "simulador.h"

static uint32_t estado = 2463534242u;

static uint32_t aleatorio(void)
{
    uint32_t x = estado;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return estado = x;
}

static int rango(int lo, int hi) /* uniforme en [lo, hi] */
{
    return lo + (int)(aleatorio() % (uint32_t)(hi - lo + 1));
}

/*
 * n procesos que llegan en [0, ventana]:
 *   - 70% cortos/interactivos (2–20 ticks de CPU)
 *   - 30% largos intensivos en CPU (30–120 ticks)
 *   - pct_io% hacen E/S: se bloquean cada 3–10 ticks de CPU por 5–20 ticks
 */
void carga_generar(pcb_t *procs, int n, uint32_t semilla, int ventana,
                   int pct_io)
{
    estado = semilla ? semilla : 2463534242u;
    for (int i = 0; i < n; i++) {
        int llegada = rango(0, ventana);
        int rafaga = rango(1, 100) <= 70 ? rango(2, 20) : rango(30, 120);
        int io_cada = 0, io_dur = 0;
        (void)rango(0, 9);
        if (rango(1, 100) <= pct_io) {
            io_cada = rango(3, 10);
            io_dur = rango(5, 20);
        }
        pcb_init(&procs[i], i + 1, llegada, rafaga, io_cada, io_dur);
    }
}

void carga_copiar(const pcb_t *o, pcb_t *d, int n)
{
    for (int i = 0; i < n; i++)
        pcb_init(&d[i], o[i].pid, o[i].llegada, o[i].rafaga, o[i].io_cada,
                 o[i].io_duracion);
}

int carga_guardar_csv(const char *ruta, const pcb_t *p, int n)
{
    FILE *f = fopen(ruta, "w");
    if (!f)
        return -1;
    fprintf(f, "pid,llegada,rafaga,io_cada,io_duracion\n");
    for (int i = 0; i < n; i++)
        fprintf(f, "%d,%d,%d,%d,%d\n", p[i].pid, p[i].llegada, p[i].rafaga,
                p[i].io_cada, p[i].io_duracion);
    fclose(f);
    return 0;
}

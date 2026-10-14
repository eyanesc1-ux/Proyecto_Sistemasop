/*
 * graficas.c — Gráficas comparativas en SVG y reporte HTML.
 *
 * Se generan directamente desde C, así que
 * funcionan igual en SO Linux y en Windows Server. Los .svg y el
 * reporte.html se abren en cualquier navegador.
 */
#include "simulador.h"

#include <math.h>
#include <string.h>

static const char *COLOR_ALG[N_ALG] = {"#4C72B0", "#DD8452", "#55A868",
                                       "#C44E52", "#937860"};
#define GRIS      "#666666"
#define REJILLA   "#E5E5E5"

/* Primitivas SVG  */
static FILE *svg_abrir(const char *dir, const char *nombre, int w, int h)
{
    char ruta[512];
    snprintf(ruta, sizeof ruta, "%s/%s", dir, nombre);
    FILE *f = fopen(ruta, "w");
    if (!f) {
        perror(ruta);
        return NULL;
    }
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" "
               "height=\"%d\" viewBox=\"0 0 %d %d\" font-family=\"Segoe UI, "
               "DejaVu Sans, Arial, sans-serif\" font-size=\"12\">\n"
               "<rect width=\"100%%\" height=\"100%%\" fill=\"white\"/>\n",
            w, h, w, h);
    return f;
}

static void svg_cerrar(FILE *f)
{
    fprintf(f, "</svg>\n");
    fclose(f);
}

static void texto(FILE *f, double x, double y, const char *anchor, int size,
                  const char *color, const char *s)
{
    fprintf(f, "<text x=\"%.1f\" y=\"%.1f\" text-anchor=\"%s\" "
               "font-size=\"%d\" fill=\"%s\">%s</text>\n",
            x, y, anchor, size, color, s);
}

static void linea(FILE *f, double x1, double y1, double x2, double y2,
                  const char *color, double ancho, const char *guiones)
{
    fprintf(f, "<line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" "
               "stroke=\"%s\" stroke-width=\"%.1f\"%s%s%s/>\n",
            x1, y1, x2, y2, color, ancho, guiones ? " stroke-dasharray=\"" : "",
            guiones ? guiones : "", guiones ? "\"" : "");
}

/* Paso para el eje Y (1, 2, 2.5, 5 x 10^k). */
static double paso_eje(double max)
{
    if (max <= 0) return 1;
    double crudo = max / 5, p = pow(10, floor(log10(crudo))), r = crudo / p;
    return (r <= 1 ? 1 : r <= 2 ? 2 : r <= 2.5 ? 2.5 : r <= 5 ? 5 : 10) * p;
}

static void formatear(char *buf, size_t n, double v, int dec)
{
    snprintf(buf, n, "%.*f", dec, v);
}

static double eje_y(FILE *f, double x, double y, double w, double h,
                    double min, double max, int dec)
{
    double paso = paso_eje(max - min);
    double tope = min + ceil((max - min) / paso) * paso;
    if (tope <= min) tope = min + paso;
    for (double v = min; v <= tope + paso * 0.01; v += paso) {
        double py = y + h - (v - min) / (tope - min) * h;
        char b[32];
        formatear(b, sizeof b, v, dec);
        linea(f, x, py, x + w, py, REJILLA, 1, NULL);
        texto(f, x - 6, py + 4, "end", 11, GRIS, b);
    }
    linea(f, x, y + h, x + w, y + h, GRIS, 1, NULL);
    return tope;
}


/* Panel de barras simples                                             */
static void panel_barras(FILE *f, double x0, double y0, double w, double h, const char *titulo, const char *const *etq, const double *v, const char *const *colores, int n, int dec, int desde_min)
{
    double x = x0 + 60, y = y0 + 34, pw = w - 75, ph = h - 70;
    double max = 0, min = 0;
    for (int i = 0; i < n; i++) if (v[i] > max) max = v[i];
    if (desde_min) {           /* throughput: valores cercanos, zoom */
        min = v[0];
        for (int i = 1; i < n; i++) if (v[i] < min) min = v[i];
        min = floor(min * 0.9);
    }
    texto(f, x0 + w / 2, y0 + 18, "middle", 14, "#222", titulo);
    double tope = eje_y(f, x, y, pw, ph, min, max * 1.08, dec);
    double ancho = pw / n;
    for (int i = 0; i < n; i++) {
        double bh = (v[i] - min) / (tope - min) * ph;
        double bx = x + i * ancho + ancho * 0.15;
        char b[32];
        fprintf(f, "<rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" "
                   "fill=\"%s\"/>\n", bx, y + ph - bh, ancho * 0.7, bh, colores[i]);
        formatear(b, sizeof b, v[i], dec);
        texto(f, bx + ancho * 0.35, y + ph - bh - 5, "middle", 11, "#222", b);
        texto(f, bx + ancho * 0.35, y + ph + 18, "middle", 12, "#222", etq[i]);
    }
}

/* Panel de líneas con eje X logarítmico (base 2) para el quantum      */
typedef struct {
    const char   *nombre;
    const char   *color;
    const double *v;
    double        referencia;   /* línea horizontal punteada (NAN = no) */
} serie_t;

static void panel_lineas_q(FILE *f, double x0, double y0, double w, double h,
                           const char *titulo, const char *ylabel,
                           const int *qs, int n, const serie_t *s, int ns,
                           double ymax_fijo, int max_rafaga, int dec)
{
    double x = x0 + 65, y = y0 + 34, pw = w - 90, ph = h - 125;
    double max = ymax_fijo;
    if (max <= 0)
        for (int k = 0; k < ns; k++)
            for (int i = 0; i < n; i++)
                if (s[k].v[i] > max) max = s[k].v[i];
    texto(f, x0 + w / 2, y0 + 18, "middle", 14, "#222", titulo);
    double tope = eje_y(f, x, y, pw, ph, 0, ymax_fijo > 0 ? max : max * 1.05, dec);
    double lmin = log2(qs[0]), lmax = log2(qs[n - 1]);
    double leyenda_x = x;
#define PX(q) (x + (log2(q) - lmin) / (lmax - lmin) * pw)
#define PY(v) (y + ph - (v) / tope * ph)

    /* Eje X: una marca por cada quantum probado */
    for (int i = 0; i < n; i++) {
        char b[16];
        snprintf(b, sizeof b, "%d", qs[i]);
        linea(f, PX(qs[i]), y + ph, PX(qs[i]), y + ph + 4, GRIS, 1, NULL);
        fprintf(f, "<text x=\"%.1f\" y=\"%.1f\" text-anchor=\"end\" font-size=\"10\" "
                   "fill=\"%s\" transform=\"rotate(-45 %.1f %.1f)\">%s</text>\n",
                PX(qs[i]), y + ph + 15, GRIS, PX(qs[i]), y + ph + 15, b);
    }
    texto(f, x + pw / 2, y + ph + 42, "middle", 12, "#222",
          "Quantum (ticks, escala logaritmica)");
    fprintf(f, "<text x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\" font-size=\"12\" "
               "fill=\"#222\" transform=\"rotate(-90 %.1f %.1f)\">%s</text>\n",
            x0 + 14, y + ph / 2, x0 + 14, y + ph / 2, ylabel);

    /* Marca: a partir de la ráfaga máxima RR se comporta como FCFS */
    if (max_rafaga > qs[0] && max_rafaga < qs[n - 1]) {
        char b[64];
        linea(f, PX(max_rafaga), y, PX(max_rafaga), y + ph, GRIS, 1, "2,3");
        snprintf(b, sizeof b, "rafaga max. = %d", max_rafaga);
        texto(f, PX(max_rafaga) - 4, y + 14, "end", 10, GRIS, b);
        texto(f, PX(max_rafaga) - 4, y + 27, "end", 10, GRIS, "RR = FCFS a la derecha");
    }

    for (int k = 0; k < ns; k++) {
        if (!isnan(s[k].referencia))
            linea(f, x, PY(s[k].referencia), x + pw, PY(s[k].referencia),
                  s[k].color, 1, "6,4");
        fprintf(f, "<polyline fill=\"none\" stroke=\"%s\" stroke-width=\"2.2\" points=\"",
                s[k].color);
        for (int i = 0; i < n; i++)
            fprintf(f, "%.1f,%.1f ", PX(qs[i]), PY(s[k].v[i]));
        fprintf(f, "\"/>\n");
        for (int i = 0; i < n; i++)
            fprintf(f, "<circle cx=\"%.1f\" cy=\"%.1f\" r=\"3\" fill=\"%s\"/>\n",
                    PX(qs[i]), PY(s[k].v[i]), s[k].color);
        /* leyenda */
        double lx = leyenda_x;
        double ly = y0 + h - 14;
        leyenda_x += 50 + 6.5 * strlen(s[k].nombre);
        linea(f, lx, ly - 4, lx + 22, ly - 4, s[k].color, 2.5, NULL);
        texto(f, lx + 28, ly, "start", 11, "#222", s[k].nombre);
    }
#undef PX
#undef PY
}

/* Gráficas                                                            */
static void grafica_comparativa(const char *dir, const fila_comparativa_t *c,
                                int n, int procesos, int cc)
{
    FILE *f = svg_abrir(dir, "g1_comparativa.svg", 1000, 720);
    if (!f) return;
    const char *etq[16], *col[16];
    double tat[16], wt[16], thr[16], rt[16];
    for (int i = 0; i < n && i < 16; i++) {
        etq[i] = c[i].etiqueta;
        col[i] = COLOR_ALG[c[i].alg];
        tat[i] = c[i].m.tat_prom;
        wt[i] = c[i].m.wt_prom;
        thr[i] = c[i].m.throughput;
        rt[i] = c[i].m.rt_prom;
    }
    char t[160];
    snprintf(t, sizeof t, "Prueba de estres: %d procesos concurrentes "
             "(cambio de contexto = %d tick)", procesos, cc);
    texto(f, 500, 24, "middle", 16, "#111", t);
    panel_barras(f, 0, 35, 500, 340, "Turnaround promedio (ticks)", etq, tat, col, n, 0, 0);
    panel_barras(f, 500, 35, 500, 340, "Waiting Time promedio en cola de listos (ticks)",
                 etq, wt, col, n, 0, 0);
    panel_barras(f, 0, 375, 500, 340, "Throughput (procesos / 1000 ticks)", etq, thr,
                 col, n, 2, 1);
    panel_barras(f, 500, 375, 500, 340, "Tiempo de respuesta promedio (ticks)", etq,
                 rt, col, n, 0, 0);
    svg_cerrar(f);
}

static void grafica_quantum(const char *dir, const fila_barrido_t *b, int n, const metricas_t *fcfs, int max_rafaga)
{
    int qs[64];
    double tat[64], wt[64], rt[64];
    if (n > 64) n = 64;
    for (int i = 0; i < n; i++) {
        qs[i] = b[i].quantum;
        tat[i] = b[i].m.tat_prom;
        wt[i] = b[i].m.wt_prom;
        rt[i] = b[i].m.rt_prom;
    }

    FILE *f = svg_abrir(dir, "g2_quantum_tiempos.svg", 1000, 480);
    if (f) {
        serie_t s[3] = {
            {"Turnaround (RR)", "#4C72B0", tat, fcfs->tat_prom},
            {"Waiting Time (RR)", "#DD8452", wt, fcfs->wt_prom},
            {"Respuesta (RR)", "#55A868", rt, fcfs->rt_prom},
        };
        panel_lineas_q(f, 0, 0, 1000, 480,
                       "Impacto del quantum en Round Robin (punteadas = FCFS)",
                       "Ticks", qs, n, s, 3, 0, max_rafaga, 0);
        svg_cerrar(f);
    }
}

static void grafica_transiciones(const char *dir, const fila_comparativa_t *c, int n)
{
    static const int idx[6][2] = {{NUEVO, LISTO}, {LISTO, EJECUCION},
                                  {EJECUCION, LISTO}, {EJECUCION, BLOQUEADO},
                                  {BLOQUEADO, LISTO}, {EJECUCION, TERMINADO}};
    static const char *nom[6] = {"NUEVO&#8594;LISTO", "LISTO&#8594;EJEC.",
                                 "EJEC.&#8594;LISTO", "EJEC.&#8594;BLOQ.",
                                 "BLOQ.&#8594;LISTO", "EJEC.&#8594;TERM."};
    FILE *f = svg_abrir(dir, "g3_transiciones.svg", 1000, 440);
    if (!f) return;
    double x = 70, y = 40, pw = 900, ph = 300, max = 0;
    for (int a = 0; a < n; a++)
        for (int t = 0; t < 6; t++)
            if (c[a].m.transiciones[idx[t][0]][idx[t][1]] > max)
                max = (double)c[a].m.transiciones[idx[t][0]][idx[t][1]];
    texto(f, 500, 22, "middle", 14, "#222",
          "Transiciones de estado administradas por el kernel en el PCB");
    double tope = eje_y(f, x, y, pw, ph, 0, max * 1.08, 0);
    double grupo = pw / 6, ancho = grupo * 0.8 / (n > 0 ? n : 1);
    for (int t = 0; t < 6; t++) {
        for (int a = 0; a < n; a++) {
            double v = (double)c[a].m.transiciones[idx[t][0]][idx[t][1]];
            double bh = v / tope * ph;
            double bx = x + t * grupo + grupo * 0.1 + a * ancho;
            fprintf(f, "<rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" "
                       "fill=\"%s\"/>\n", bx, y + ph - bh, ancho * 0.92, bh,
                    COLOR_ALG[c[a].alg]);
        }
        texto(f, x + t * grupo + grupo / 2, y + ph + 18, "middle", 12, "#222", nom[t]);
    }
    for (int a = 0; a < n; a++) {
        double lx = x + 20 + a * 170;
        fprintf(f, "<rect x=\"%.1f\" y=\"%.1f\" width=\"14\" height=\"14\" fill=\"%s\"/>\n",
                lx, y + ph + 50, COLOR_ALG[c[a].alg]);
        texto(f, lx + 20, y + ph + 62, "start", 12, "#222", c[a].etiqueta);
    }
    svg_cerrar(f);
}

/* Reporte HTML                                                        */
static void reporte_html(const char *dir, const fila_comparativa_t *c, int nc,
                         const fila_barrido_t *b, int nb, int procesos, int cc,
                         const char *equipo)
{
    char ruta[512];
    snprintf(ruta, sizeof ruta, "%s/reporte.html", dir);
    FILE *f = fopen(ruta, "w");
    if (!f) {
        perror(ruta);
        return;
    }
    fprintf(f,
        "<!doctype html>\n<html lang=\"es\"><head><meta charset=\"utf-8\">\n"
        "<title>Simulador de planificaci&#243;n - Fase 2</title>\n<style>\n"
        "body{font-family:Segoe UI,DejaVu Sans,Arial,sans-serif;margin:24px auto;"
        "max-width:1040px;color:#222;background:#fff}\n"
        "h1{font-size:22px}h2{font-size:17px;margin-top:32px}\n"
        "table{border-collapse:collapse;font-size:13px;margin:8px 0}\n"
        "th,td{border:1px solid #ccc;padding:4px 9px;text-align:right}\n"
        "th{background:#f2f2f2}td:first-child,th:first-child{text-align:left}\n"
        "img{max-width:100%%;border:1px solid #eee;margin:8px 0}\n"
        ".info{color:#555;font-size:13px}\n</style></head><body>\n"
        "<h1>Simulador de planificaci&#243;n de CPU &#8212; Fase 2</h1>\n"
        "<p class=\"info\">Equipo: %s &#183; %d procesos concurrentes &#183; "
        "cambio de contexto = %d tick</p>\n", equipo, procesos, cc);

    fprintf(f, "<h2>Comparativa de algoritmos (prueba de estr&#233;s)</h2>\n"
               "<table><tr><th>Algoritmo</th><th>Turnaround prom.</th>"
               "<th>Waiting Time prom.</th><th>Respuesta prom.</th>"
               "<th>Throughput (proc/1000 t)</th><th>CPU &#250;til %%</th>"
               "<th>Cambio de contexto %%</th><th>Cambios de contexto</th>"
               "<th>Terminados</th></tr>\n");
    for (int i = 0; i < nc; i++) {
        const metricas_t *m = &c[i].m;
        fprintf(f, "<tr><td>%s</td><td>%.2f</td><td>%.2f</td><td>%.2f</td>"
                   "<td>%.3f</td><td>%.2f</td><td>%.2f</td><td>%ld</td>"
                   "<td>%d/%d</td></tr>\n", c[i].etiqueta, m->tat_prom,
                m->wt_prom, m->rt_prom, m->throughput, m->utilizacion,
                m->sobrecarga, m->cambios_contexto, m->finalizados, m->n);
    }
    fprintf(f, "</table>\n<img src=\"graficas/g1_comparativa.svg\" "
               "alt=\"Comparativa\">\n"
               "<img src=\"graficas/g5_transiciones.svg\" alt=\"Transiciones\">\n");

    fprintf(f, "<h2>Barrido del quantum de Round Robin</h2>\n"
               "<table><tr><th>Quantum</th><th>Turnaround prom.</th>"
               "<th>Waiting Time prom.</th><th>Respuesta prom.</th>"
               "<th>Throughput</th><th>Cambio de contexto %%</th>"
               "<th>Cambios de contexto</th></tr>\n");
    for (int i = 0; i < nb; i++) {
        const metricas_t *m = &b[i].m;
        fprintf(f, "<tr><td>%d</td><td>%.2f</td><td>%.2f</td><td>%.2f</td>"
                   "<td>%.3f</td><td>%.2f</td><td>%ld</td></tr>\n",
                b[i].quantum, m->tat_prom, m->wt_prom, m->rt_prom,
                m->throughput, m->sobrecarga, m->cambios_contexto);
    }
    fprintf(f, "</table>\n"
               "<img src=\"graficas/g2_quantum_tiempos.svg\" alt=\"Quantum\">\n"
               "<p class=\"info\">Bit&#225;coras de eventos del kernel: carpeta "
               "<code>bitacoras/</code>. Datos: <code>comparativa.csv</code>, "
               "<code>barrido_quantum.csv</code>, <code>carga.csv</code>.</p>\n"
               "</body></html>\n");
    fclose(f);
}

void graficas_generar(const char *dir, const fila_comparativa_t *comp,
                      int n_comp, const fila_barrido_t *barr, int n_barr,
                      const metricas_t *fcfs, int procesos, int cc,
                      int max_rafaga, const char *equipo)
{
    char sub[512];
    snprintf(sub, sizeof sub, "%s/graficas", dir);
    grafica_comparativa(sub, comp, n_comp, procesos, cc);
    grafica_transiciones(sub, comp, n_comp);
    if (n_barr >= 2)
        grafica_quantum(sub, barr, n_barr, fcfs, max_rafaga);
    reporte_html(dir, comp, n_comp, barr, n_barr, procesos, cc, equipo);
}

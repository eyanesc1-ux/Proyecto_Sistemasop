#include <iostream>
#include <vector>

struct Proceso {
    int id;
    int tiempoRáfaga;
    int tiempoEspera;
    int tiempoRetorno;
};

void calcularFCFS(std::vector<Proceso>& procesos) {
    int n = procesos.size();
    procesos[0].tiempoEspera = 0;

    for (int i = 1; i < n; i++) {
        procesos[i].tiempoEspera = procesos[i - 1].tiempoEspera + procesos[i - 1].tiempoRáfaga;
    }

    for (int i = 0; i < n; i++) {
        procesos[i].tiempoRetorno = procesos[i].tiempoEspera + procesos[i].tiempoRáfaga;
    }
}

int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "  SIMULADOR DE PLANIFICACION DE PROCESOS   " << std::endl;
    std::cout << "  Algoritmo: FCFS (First-Come, First-Served)" << std::endl;
    std::cout << "============================================" << std::endl << std::endl;

    std::vector<Proceso> procesos = {
        {1, 6, 0, 0},
        {2, 8, 0, 0},
        {3, 4, 0, 0},
        {4, 3, 0, 0}
    };

    calcularFCFS(procesos);

    double totalEspera = 0, totalRetorno = 0;
    std::cout << "ID\tRáfaga\tEspera\tRetorno" << std::endl;
    std::cout << "------------------------------------" << std::endl;

    for (const auto& p : procesos) {
        totalEspera += p.tiempoEspera;
        totalRetorno += p.tiempoRetorno;
        std::cout << p.id << "\t" << p.tiempoRáfaga << "\t" 
                  << p.tiempoEspera << "\t" << p.tiempoRetorno << std::endl;
    }

    std::cout << std::endl;
    std::cout << "Promedio Tiempo de Espera: " << (totalEspera / procesos.size()) << " ms" << std::endl;
    std::cout << "Promedio Tiempo de Retorno: " << (totalRetorno / procesos.size()) << " ms" << std::endl;

    return 0;
}

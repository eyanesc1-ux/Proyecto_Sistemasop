#include <iostream>
#include <thread>
#include <mutex>
#include <vector>

std::mutex cerrojo;
int contadorCompartido = 0;

void incrementarContador(int idHilo, int iteraciones) {
    for (int i = 0; i < iteraciones; ++i) {
        std::lock_guard<std::mutex> guard(cerrojo);
        contadorCompartido++;
    }
}

int main() {
    std::cout << "--- Demostración de Concurrencia y Sincronización ---" << std::endl;
    
    const int NUM_HILOS = 5;
    const int ITERACIONES = 10000;
    std::vector<std::thread> hilos;

    for (int i = 0; i < NUM_HILOS; ++i) {
        hilos.push_back(std::thread(incrementarContador, i, ITERACIONES));
    }

    for (auto& h : hilos) {
        h.join();
    }

    std::cout << "Valor esperado del contador: " << (NUM_HILOS * ITERACIONES) << std::endl;
    std::cout << "Valor final del contador:    " << contadorCompartido << std::endl;

    return 0;
}

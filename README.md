# Proyecto Final de Sistemas Operativos y Redes

Repositorio oficial para el proyecto de Sistemas Operativos. Incluye la simulación de algoritmos de planificación de procesos, ejercicios de concurrencia e hilos, scripts de gestión de directorios y la arquitectura de red VPN WireGuard (Hub-and-Spoke en Azure).

##  Estructura del Repositorio

- /simulador-procesos: Código fuente en C++ para la simulación del algoritmo FCFS.
- /concurrencia: Ejemplo práctico de sincronización con hilos y exclusión mutua en C++.
- /red-wireguard: Configuración de la topología VPN desplegada en Azure.
- /directorios: Script Bash de automatización de carpetas.

# Quantum-QA Híbrido

Proyecto integrador enfocado en la implementación de una infraestructura híbrida segura y eficiente, combinando servicios locales en Windows Server con entornos distribuidos Linux mediante redes privadas virtuales (WireGuard VPN) y módulos de simulación y concurrencia.

## Estructura del Repositorio

* **`concurrencia/`**: Contiene scripts y pruebas de sincronización y control de procesos (ej. `semaforos.cpp`).
* **`docs/`**: Documentación formal del proyecto y diagramas técnicos (`Documentacion.docx`).
* **`red-wireguard/`**: Archivos de configuración de ejemplo (`wg0.conf.example`), puntos de montaje y respaldos de la red VPN del equipo.
* **`simulador-procesos/`**: Núcleo del motor de procesamiento y análisis de autómatas/procesos.
  * `src/` & `include/`: Código fuente en C/C++ o C#.
  * `bin/`: Archivos binarios y ejecutables compilados.
  * Casos de prueba (`milprocesos/`, `quantumgrande/`, `quantumpequenio/`).
  * Scripts de automatización: `Makefile`, `ejecutar.sh` y `ejecutar.bat`.

##  Requisitos y Ejecución

### 1. Compilación y Ejecución del Simulador
Puedes utilizar las herramientas automatizadas provistas en la carpeta del simulador:
* **En Linux (Bash):**
```bash
  cd simulador-procesos
  ./ejecutar.sh
## 🔒 Configuración de Red (WireGuard VPN)

El proyecto utiliza una red privada virtual basada en WireGuard para la comunicación cifrada y el intercambio de recursos entre los nodos del equipo (`10.7.0.0/24`). 

A continuación se muestra una plantilla de ejemplo basada en la configuración del servidor (`wg0.conf.example`) con las claves públicas y direcciones IP asignadas a cada participante:

```ini
[Interface]
Address = 10.7.0.1/24
ListenPort = 51820
PrivateKey = <TU_CLAVE_PRIVADA_DEL_SERVIDOR>

PostUp = iptables -A FORWARD -i wg0 -j ACCEPT; iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
PostDown = iptables -D FORWARD -i wg0 -j ACCEPT; iptables -t nat -D POSTROUTING -o eth0 -j MASQUERADE

# Persona 1
[Peer]
PublicKey = B0UMEuKbFkuVM0YhzJz3KTyRjJ7cslwtWOP4wKr9Shk=
AllowedIPs = 10.7.0.8/32

# Persona 2 (Nuevo WinServer)
[Peer]
PublicKey = 3KxqnW0NaX9HJZstCzvwrRN0QhDcd77kWgzaLJ7jpqgo=
AllowedIPs = 10.7.0.5/32

# Persona 3 (Alexander - WS22)
[Peer]
PublicKey = Sys0hbEVczye/hPgZG9Iv4nI4YPirdR9qGcDiCbJhs=
AllowedIPs = 10.7.0.50/32

# Persona 3 (Alexander - W11)
[Peer]
PublicKey = 4kyn8I4rn26s0oQlsiuHNNRegsjdLdjmUS9R/K3nzUU=
AllowedIPs = 10.7.0.60/32

# Persona 4 (Windows Server)
[Peer]
PublicKey = 13AiUrzDJsJOi8PHAccznMQ8maWdWZjlbt30nwG1kA=
AllowedIPs = 10.7.0.55/32

# Persona 5 (David PC)
[Peer]
PublicKey = 0HXxSVov/mqkUtbhBuOpargWkn02mEiAVcrX5XDQ5hc=
AllowedIPs = 10.7.0.6/32

# Persona 6 (Kathy)
[Peer]
PublicKey = G/aV6sG2+47BHMyscU2eLHTcmZiFjlV08MJHrkWYGVg=
AllowedIPs = 10.7.0.16/32
  ```bash
  cd simulador-procesos
  ./ejecutar.sh

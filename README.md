# Práctica 2: Protocolo de Transporte 

Este repositorio contiene la implementación de un protocolo de control de flujo y errores sobre UDP, desarrollado para la asignatura **Redes de Ordenadores**. El objetivo es garantizar una comunicación fiable, sin pérdidas ni duplicados, incluso en entornos con alta latencia.

## 🚀 Características
* **Control de Flujo:** Implementación de Parada y Espera (Stop & Wait) y Ventana Deslizante (Sliding Window/Go-Back-N).
* **Control de Errores:** Mecanismo ARQ mediante confirmaciones (ACKs) y retransmisión por expiración de temporizador (Timeout).
* **Integridad:** Validación de paquetes recibidos mediante la API de Checksum del framework.

## 🛠️ Configuración del Entorno 
En mi caso como no realice la práctica en el laboratorio, para cumplir con el requisito de comunicación entre dos equipos independientes, se utilizaron dos máquinas virtuales con Ubuntu.

### 1. Configuración de Red
Ambas máquinas se configuraron en modo **Red Interna** para simular un enlace físico punto a punto.

### 2. Asignación de Direcciones IP
Al no disponer de un servidor DHCP en la red aislada, las direcciones se asignaron manualmente mediante la terminal:
* **VM 1:** `sudo ip addr add 192.168.1.1/24 dev enp0s3 && sudo ip link set enp0s3 up`
* **VM 2:** `sudo ip addr add 192.168.1.2/24 dev enp0s3 && sudo ip link set enp0s3 up`

## 3. Compilación
El proyecto utiliza un `Makefile` para gestionar la compilación de todos los archivos `.c` (incluyendo `rlib.c` y `reliable.c`):
```bash
make clean    # Elimina ejecutables previos
make          # Compila y genera el archivo 'reliable'
```

### 💻 Ejecución del Programa
El programa requiere indicar el puerto local de escucha y la dirección (IP:Puerto) del destino para establecer la comunicación[cite: 5].

#### 1. Modo Consola (Chat bidireccional)
Permite el intercambio manual de mensajes de texto a través de la terminal[cite: 5].

* **VM 1 (IP 192.168.1.1):**
    ```bash
    ./reliable 5555 192.168.1.2:6666
    ```
* **VM 2 (IP 192.168.1.2):**
    ```bash
    ./reliable 6666 192.168.1.1:5555
    ``` 

#### 2. Modo Sintético (Prueba de Rendimiento)
Genera ráfagas de datos de forma automática para medir el *throughput* (rendimiento efectivo) del enlace[cite: 5, 8].

* **VM 1:** ```bash
    ./reliable 5555 192.168.1.2:6666 -w 5 -s
    ``` 
* **VM 2:** ```bash
    ./reliable 6666 192.168.1.1:5555 -w 5 -s
    ``` 

---

### ⚙️ Parámetros de Simulación
El framework permite configurar diversos parámetros para modelar el comportamiento del protocolo de control de flujo:

| Opción | Descripción | Comportamiento |
| :--- | :--- | :--- |
| **-w W** | Tamaño de la ventana | Define cuántos paquetes puede enviar el emisor sin haber recibido aún su ACK. |
| **-t T** | Timeout | Tiempo de espera en nanosegundos antes de retransmitir una trama (por defecto: 10 ms). |
| **-e E** | Porcentaje de errores |Probabilidad (0-100%) de que una trama se corrompa aleatoriamente durante el tránsito. |
| **-s** | Tráfico Sintético | Activa el generador de tráfico que envía paquetes a la mayor velocidad posible. |
| **-d D** | Nivel de Debug | Imprime mensajes de depuración en colores con verbosidad de 1 a 3. |


### Documentación 
- [Diagrama de flujo](./documentation/)
- [Desarrollo código](./21_reliable/reliable.c)
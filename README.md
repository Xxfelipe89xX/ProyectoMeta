# EVRP — Electric Vehicle Routing Problem

Primer paso para la solución del EVRP.
El objetivo es minimizar el consumo energético total (kWh) al atender a todos los clientes, respetando restricciones de capacidad de carga, autonomía de batería y estaciones de recarga.
Por ahora tenemos un greedy determinista para tener una primera solucion factible.
## Estructura del proyecto

```
ProyectoMeta/
├── include/            # Headers (.hpp)
│   ├── instance.hpp
│   ├── energy.hpp
│   ├── solution.hpp
│   └── constructive.hpp
├── src/                # Código fuente (.cpp)
│   ├── main.cpp
│   ├── instance.cpp
│   ├── energy.cpp
│   ├── solution.cpp
│   └── constructive.cpp
├── generator/
│   └── generador.py    # Generador de instancias
└── instances/          # Instancias generadas (no versionadas)
    ├── small/
    ├── large/
    └── json/
```

## Requisitos

- **g++** con soporte para C++17 (ej. MinGW / MSYS2 en Windows)
- **Python 3** (para generar las instancias)

## Paso a paso

### 1. Generar las instancias

```bash
python3 generator/generador.py
```

Esto crea 55 instancias (15 pequeñas + 40 grandes) dentro de la carpeta `instances/`.

### 2. Compilar el proyecto

```bash
g++ -std=c++17 -O3 -Wall -Wextra -Iinclude src/main.cpp src/instance.cpp src/energy.cpp src/solution.cpp src/constructive.cpp -o evrp
```

### 3. Ejecutar con una instancia

```bash
./evrp instances/small/C10R2.txt
```

La salida mostrará las rutas generadas, energía consumida, emisiones de CO₂ y un detalle arco por arco de cada ruta.

## ¿Qué hace el código?

1. **Lectura de instancia** (`instance.cpp`): parsea el archivo `.txt` con la ubicación del depósito, clientes, estaciones de recarga y parámetros del vehículo.
2. **Modelo de energía** (`energy.cpp`): calcula el consumo energético y las emisiones de cada arco según distancia, velocidad, peso del vehículo y carga transportada.
3. **Heurística constructiva** (`constructive.cpp`): construye rutas de forma greedy evaluando, para cada cliente no visitado, la mejor forma de insertarlo (con o sin parada en estaciones de recarga), seleccionando el movimiento de menor consumo energético.
4. **Evaluación de solución** (`solution.cpp`): verifica factibilidad (batería, capacidad, cobertura de clientes) y calcula los totales de energía, distancia y emisiones.

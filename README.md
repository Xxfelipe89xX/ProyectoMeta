# EVRP — Electric Vehicle Routing Problem

Primer paso para la solución del EVRP.
El objetivo es minimizar el consumo energético total (kWh) al atender a todos los clientes, respetando restricciones de capacidad de carga, autonomía de batería y estaciones de recarga.
El enfoque actual incluye una fase constructiva greedy (determinista y aleatorizada), una fase de reparación de clientes faltantes, una búsqueda local para mejorar la factibilidad y reducir el consumo energético, y un generador multi-solución que produce un pool diverso de soluciones candidatas.

## Estructura del proyecto

```text
ProyectoMeta/
├── include/           
│   ├── instance.hpp
│   ├── energy.hpp
│   ├── solution.hpp
│   ├── constructive.hpp
│   ├── repair.hpp
│   ├── local_search.hpp
│   └── multi_greedy.hpp
├── src/               
│   ├── main.cpp
│   ├── instance.cpp
│   ├── energy.cpp
│   ├── solution.cpp
│   ├── constructive.cpp
│   ├── repair.cpp
│   ├── local_search.cpp
│   └── multi_greedy.cpp
├── generator/
│   └── generador.py    
├── instances/          
│   ├── small/
│   ├── large/
│   └── json/
├── output/             
│   └── results/
└── run_batch.ps1       
```

## Requisitos

- **g++** con soporte para C++17 (ej. MinGW / MSYS2 en Windows)
- **Python 3** (para generar las instancias)
- **PowerShell** (para usar el script de pruebas masivas)

## Paso a paso

### 1. Generar las instancias

```bash
python3 generator/generador.py
```

Esto crea las instancias dentro de la carpeta `instances/` (clasificadas en pequeñas y grandes).

### 2. Compilar el proyecto

```powershell
g++ -std=c++17 -O3 -Wall -Wextra -Iinclude src/*.cpp -o evrp
```

### 3. Ejecutar de forma individual

Ejecución estándar con detalle paso a paso:
```powershell
./evrp instances/small/C10R2.txt
```

Ejecución para obtener una sola línea de resumen:
```powershell
./evrp --summary instances/small/C10R2.txt
```

### 4. Ejecutar en modo multi-greedy

Genera un pool de múltiples soluciones diversas usando la heurística constructiva aleatorizada con selección top-k:
```powershell
./evrp --multi-greedy instances/small/C12R2.txt
```

Este modo construye varias soluciones independientes seleccionando aleatoriamente entre los `top_k` mejores candidatos en cada paso del greedy, aplicando opcionalmente reparación y búsqueda local a cada una. Las soluciones se ordenan por factibilidad y consumo energético.

### 5. Ejecutar lote de pruebas (Batch)

Para probar automáticamente todas las instancias y obtener métricas globales, utiliza el script de PowerShell:

```powershell
.\run_batch.ps1
```

Este script iterará sobre las carpetas `small` y `large`, llamará a `./evrp --summary` y agrupará todos los resultados en archivos CSV y de resumen dentro de la carpeta `output/results/`.

## ¿Qué hace el código actualmente?

1. **Lectura de instancia** (`instance.cpp`): parsea el archivo `.txt` con la ubicación del depósito, clientes, estaciones de recarga y parámetros del vehículo.
2. **Modelo de energía** (`energy.cpp`): calcula el consumo energético y las emisiones de cada arco según distancia, velocidad, peso del vehículo y carga transportada.
3. **Heurística constructiva** (`constructive.cpp`): construye rutas iniciales de forma greedy. Ofrece dos variantes:
   - **Determinista** (`greedy_constructive`): siempre selecciona el mejor candidato.
   - **Aleatorizada** (`greedy_constructive_randomized`): selecciona aleatoriamente entre los `top_k` mejores candidatos en cada paso, permitiendo generar soluciones diversas.
4. **Fase de reparación** (`repair.cpp`): identifica clientes que no fueron visitados e intenta reinsertarlos en rutas existentes o nuevas para buscar la factibilidad total.
5. **Búsqueda local** (`local_search.cpp`): aplica operadores de mejora (como *relocate* intra-ruta limitados) buscando reducir el consumo energético.
6. **Evaluación de solución** (`solution.cpp`): verifica la factibilidad (batería, capacidad, cobertura de clientes) y reporta totales de energía, distancia y emisiones.
7. **Generador multi-solución** (`multi_greedy.cpp`): genera un pool de soluciones diversas usando la constructiva aleatorizada, eliminando duplicados por firma de ruta, y aplicando opcionalmente reparación y búsqueda local a cada una.

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
│   ├── results/
│   ├── results_multigreedy/
│   └── multigreedy_solutions/
├── run_batch.ps1       
└── run_batch_multigreedy.ps1
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

Este modo construye múltiples soluciones independientes seleccionando aleatoriamente entre los `top_k` mejores candidatos en cada paso del greedy, aplicando reparación y búsqueda local. Para asegurar la idoneidad como población inicial de algoritmos evolutivos (como **Bee Colony**), el pool de 10 soluciones resultantes se divide automáticamente en **al menos 7 soluciones factibles** y **al menos 3 soluciones infactibles** (o el mejor ratio posible).
Las soluciones se guardan de forma serializada en formato estructurado JSON dentro del directorio `output/multigreedy_solutions/[nombre_instancia]_solutions.json`.

### 5. Ejecutar lotes de pruebas (Batch)

#### A. Ejecución de lote determinista estándar:
```powershell
.\run_batch.ps1
```
Este script iterará sobre las carpetas `small` y `large`, llamará a `./evrp --summary` y agrupará todos los resultados en archivos CSV y de resumen dentro de la carpeta `output/results/`.

#### B. Ejecución de lote multi-greedy para Bee Colony:
```powershell
.\run_batch_multigreedy.ps1
```
Este script recorre todas las instancias corriendo el modo `--multi-greedy`. Exporta las métricas de rendimiento consolidadas en `output/results_multigreedy/` y deposita las secuencias detalladas de rutas en formato JSON en `output/multigreedy_solutions/`.

## ¿Qué hace el código actualmente?

1. **Lectura de instancia** (`instance.cpp`): parsea el archivo `.txt` con la ubicación del depósito, clientes, estaciones de recarga y parámetros del vehículo.
2. **Modelo de energía** (`energy.cpp`): calcula el consumo energético y las emisiones de cada arco según distancia, velocidad, peso del vehículo y carga transportada.
3. **Heurística constructiva** (`constructive.cpp`): construye rutas iniciales de forma greedy. Ofrece dos variantes:
   - **Determinista** (`greedy_constructive`): siempre selecciona el mejor candidato.
   - **Aleatorizada** (`greedy_constructive_randomized`): selecciona aleatoriamente entre los `top_k` mejores candidatos en cada paso, permitiendo generar soluciones diversas.
4. **Fase de reparación** (`repair.cpp`): identifica clientes que no fueron visitados e intenta reinsertarlos en rutas existentes o nuevas para buscar la factibilidad total.
5. **Búsqueda local** (`local_search.cpp`): aplica operadores de mejora (*relocate* intra-ruta optimizado de paso simple) buscando reducir el consumo energético de las rutas válidas.
6. **Evaluación de solución** (`solution.cpp`): verifica la factibilidad (batería, capacidad, cobertura de clientes) y reporta totales de energía, distancia y emisiones.
7. **Generador multi-solución** (`multi_greedy.cpp`): genera un pool balanceado de soluciones diversas usando la constructiva aleatorizada (manteniendo un split de 7/3 entre factibles e infactibles), eliminando duplicados por firma de ruta, y guardándolos en formato JSON.

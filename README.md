# EVRP — Electric Vehicle Routing Problem

Primer paso para la solución del EVRP.
El objetivo es minimizar el consumo energético total (kWh) al atender a todos los clientes, respetando restricciones de capacidad de carga, autonomía de batería y estaciones de recarga.
El enfoque actual incluye una fase constructiva greedy, una fase de reparación de clientes faltantes y una búsqueda local para mejorar la factibilidad y reducir el consumo energético.

## Estructura del proyecto

```text
ProyectoMeta/
├── include/           
│   ├── instance.hpp
│   ├── energy.hpp
│   ├── solution.hpp
│   ├── constructive.hpp
│   ├── repair.hpp
│   └── local_search.hpp
├── src/               
│   ├── main.cpp
│   ├── instance.cpp
│   ├── energy.cpp
│   ├── solution.cpp
│   ├── constructive.cpp
│   ├── repair.cpp
│   └── local_search.cpp
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

### 4. Ejecutar lote de pruebas (Batch)

Para probar automáticamente todas las instancias y obtener métricas globales, utiliza el script de PowerShell:

```powershell
.\run_batch.ps1
```

Este script iterará sobre las carpetas `small` y `large`, llamará a `./evrp --summary` y agrupará todos los resultados en archivos CSV y de resumen dentro de la carpeta `output/results/`.

## ¿Qué hace el código actualmente?

1. **Lectura de instancia** (`instance.cpp`): parsea el archivo `.txt` con la ubicación del depósito, clientes, estaciones de recarga y parámetros del vehículo.
2. **Modelo de energía** (`energy.cpp`): calcula el consumo energético y las emisiones de cada arco según distancia, velocidad, peso del vehículo y carga transportada.
3. **Heurística constructiva** (`constructive.cpp`): construye rutas iniciales de forma greedy.
4. **Fase de reparación** (`repair.cpp`): identifica clientes que no fueron visitados e intenta reinsertarlos en rutas existentes o nuevas para buscar la factibilidad total.
5. **Búsqueda local** (`local_search.cpp`): aplica operadores de mejora (como *relocate* intra-ruta o inter-ruta limitados) buscando reducir el consumo energético o corregir pequeñas desviaciones.
6. **Evaluación de solución** (`solution.cpp`): verifica la factibilidad (batería, capacidad, cobertura de clientes) y reporta totales de energía, distancia y emisiones.

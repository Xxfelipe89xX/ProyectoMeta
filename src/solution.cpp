#include "../include/solution.hpp"
#include "../include/energy.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_map>

static std::string node_type_to_string(NodeType t) {
  switch (t) {
  case NodeType::DEPOT_START:
    return "DEPOT_START";
  case NodeType::DEPOT_END:
    return "DEPOT_END";
  case NodeType::CUSTOMER:
    return "CUSTOMER";
  case NodeType::STATION:
    return "STATION";
  default:
    return "UNKNOWN";
  }
}

static std::string node_label(const Instance &inst, int node_id) {
  const Node &n = inst.nodes[node_id];
  std::ostringstream oss;
  oss << node_id << " [orig=" << n.original_id << ", "
      << node_type_to_string(n.type) << "]";
  return oss.str();
}

double route_total_demand(const Instance &inst, const std::vector<int> &route) {
  double total = 0.0;
  for (int node : route) {
    if (is_customer(inst, node)) {
      total += inst.nodes[node].demand;
    }
  }
  return total;
}

RouteEvaluation evaluate_route(const Instance &inst,
                               const std::vector<int> &route) {
  RouteEvaluation eval;

  if (route.size() < 2) {
    eval.feasible = false;
    eval.error_message = "Ruta demasiado corta.";
    return eval;
  }

  if (!is_depot_start(inst, route.front())) {
    eval.feasible = false;
    eval.error_message = "La ruta no comienza en DEPOT_START.";
    return eval;
  }

  if (!is_depot_end(inst, route.back())) {
    eval.feasible = false;
    eval.error_message = "La ruta no termina en DEPOT_END.";
    return eval;
  }

  eval.total_demand_tons = route_total_demand(inst, route);

  if (eval.total_demand_tons > inst.vehicle_capacity_tons + 1e-9) {
    eval.feasible = false;
    std::ostringstream oss;
    oss << "Capacidad excedida. Demanda ruta = " << eval.total_demand_tons
        << " tons, capacidad = " << inst.vehicle_capacity_tons << " tons.";
    eval.error_message = oss.str();
    return eval;
  }

  double remaining_load = eval.total_demand_tons;
  double battery = inst.battery_capacity_kwh;

  for (size_t i = 0; i + 1 < route.size(); ++i) {
    int u = route[i];
    int v = route[i + 1];

    ArcDetail arc;
    arc.from = u;
    arc.to = v;
    arc.distance_miles = inst.distance_miles[u][v];
    arc.speed_kmh = inst.speed_kmh[u][v];
    arc.load_before_tons = remaining_load;

    if (is_depot_start(inst, u) || is_station(inst, u)) {
      battery = inst.battery_capacity_kwh;
      arc.recharge_before_arc = true;
    }

    arc.battery_before_kwh = battery;
    arc.energy_used_kwh = arc_energy_kwh(inst, u, v, remaining_load);
    arc.emissions_kg = arc_emissions_kg(inst, u, v, remaining_load);

    if (arc.energy_used_kwh > battery + 1e-9) {
      arc.feasible = false;
      std::ostringstream oss;
      oss << "Bateria insuficiente en arco " << node_label(inst, u) << " -> "
          << node_label(inst, v)
          << ". Energia requerida = " << arc.energy_used_kwh
          << " kWh, bateria disponible = " << battery << " kWh.";
      arc.error_message = oss.str();

      eval.arcs.push_back(arc);
      eval.feasible = false;
      eval.error_message = arc.error_message;
      return eval;
    }

    battery -= arc.energy_used_kwh;
    arc.battery_after_kwh = battery;

    if (is_customer(inst, v)) {
      remaining_load -= inst.nodes[v].demand;
    }

    if (remaining_load < -1e-9) {
      arc.feasible = false;
      std::ostringstream oss;
      oss << "Carga negativa luego de visitar " << node_label(inst, v) << ".";
      arc.error_message = oss.str();

      eval.arcs.push_back(arc);
      eval.feasible = false;
      eval.error_message = arc.error_message;
      return eval;
    }

    arc.load_after_tons = remaining_load;

    eval.total_distance_miles += arc.distance_miles;
    eval.total_energy_kwh += arc.energy_used_kwh;
    eval.total_emissions_kg += arc.emissions_kg;

    eval.arcs.push_back(arc);
  }

  return eval;
}

Solution evaluate_solution(const Instance &inst, const Solution &sol) {
  Solution result = sol;
  result.total_energy_kwh = 0.0;
  result.total_emissions_kg = 0.0;
  result.total_distance_miles = 0.0;
  result.feasible = true;
  result.error_message.clear();

  if ((int)result.routes.size() > inst.max_vehicles) {
    result.feasible = false;
    std::ostringstream oss;
    oss << "Se excede el maximo de vehiculos. Rutas = " << result.routes.size()
        << ", maximo = " << inst.max_vehicles << ".";
    result.error_message = oss.str();
    return result;
  }

  std::unordered_map<int, int> customer_visit_count;

  for (const auto &route : result.routes) {
    RouteEvaluation re = evaluate_route(inst, route);

    result.total_distance_miles += re.total_distance_miles;
    result.total_energy_kwh += re.total_energy_kwh;
    result.total_emissions_kg += re.total_emissions_kg;

    if (!re.feasible) {
      result.feasible = false;
      result.error_message = re.error_message;
      return result;
    }

    for (int node : route) {
      if (is_customer(inst, node)) {
        customer_visit_count[node]++;
      }
    }
  }

  std::vector<int> missing_customers;
  std::vector<int> repeated_customers;

  for (int c : inst.customer_ids) {
    int count = customer_visit_count[c];
    if (count == 0)
      missing_customers.push_back(c);
    if (count > 1)
      repeated_customers.push_back(c);
  }

  if (!repeated_customers.empty()) {
    result.feasible = false;
    std::ostringstream oss;
    oss << "Clientes repetidos: ";
    for (size_t i = 0; i < repeated_customers.size(); ++i) {
      if (i)
        oss << ", ";
      oss << node_label(inst, repeated_customers[i]);
    }
    result.error_message = oss.str();
    return result;
  }

  if (!missing_customers.empty()) {
    result.feasible = false;
    std::ostringstream oss;
    oss << "No todos los clientes fueron visitados. Faltan: ";
    for (size_t i = 0; i < missing_customers.size(); ++i) {
      if (i)
        oss << ", ";
      oss << node_label(inst, missing_customers[i]);
    }
    result.error_message = oss.str();
    return result;
  }

  return result;
}

static void print_route_summary(const Instance &inst,
                                const std::vector<int> &route, int route_idx) {
  RouteEvaluation re = evaluate_route(inst, route);

  std::cout << "Ruta " << route_idx + 1 << ":\n";
  std::cout << "  Secuencia: ";
  for (size_t i = 0; i < route.size(); ++i) {
    std::cout << route[i];
    if (i + 1 < route.size())
      std::cout << " -> ";
  }
  std::cout << "\n";

  std::cout << "  Secuencia detallada:\n";
  for (size_t i = 0; i < route.size(); ++i) {
    const Node &n = inst.nodes[route[i]];
    std::cout << "    - " << route[i] << " [orig=" << n.original_id
              << ", type=" << node_type_to_string(n.type)
              << ", demand=" << n.demand << "]\n";
  }

  std::cout << "  Demanda total ruta (tons): " << re.total_demand_tons << "\n";
  std::cout << "  Distancia total ruta (miles): " << re.total_distance_miles
            << "\n";
  std::cout << "  Energia total ruta (kWh): " << re.total_energy_kwh << "\n";
  std::cout << "  Emisiones ruta (kg CO2): " << re.total_emissions_kg << "\n";
  std::cout << "  Factible: " << (re.feasible ? "si" : "no") << "\n";

  if (!re.feasible) {
    std::cout << "  Error ruta: " << re.error_message << "\n";
  }

  std::cout << "  Arcos:\n";
  for (size_t i = 0; i < re.arcs.size(); ++i) {
    const ArcDetail &a = re.arcs[i];
    std::cout << "    Arco " << i + 1 << ": " << node_label(inst, a.from)
              << " -> " << node_label(inst, a.to) << "\n";
    std::cout << "      Distancia (miles): " << a.distance_miles << "\n";
    std::cout << "      Velocidad (km/h): " << a.speed_kmh << "\n";
    std::cout << "      Carga antes (tons): " << a.load_before_tons << "\n";
    std::cout << "      Carga despues (tons): " << a.load_after_tons << "\n";
    std::cout << "      Recarga antes del arco: "
              << (a.recharge_before_arc ? "si" : "no") << "\n";
    std::cout << "      Bateria antes (kWh): " << a.battery_before_kwh << "\n";
    std::cout << "      Energia usada (kWh): " << a.energy_used_kwh << "\n";
    std::cout << "      Bateria despues (kWh): " << a.battery_after_kwh << "\n";
    std::cout << "      Emisiones (kg CO2): " << a.emissions_kg << "\n";
    std::cout << "      Factible arco: " << (a.feasible ? "si" : "no") << "\n";
    if (!a.feasible) {
      std::cout << "      Error arco: " << a.error_message << "\n";
    }
  }
}

void print_solution(const Instance &inst, const Solution &sol) {
  Solution evaluated = evaluate_solution(inst, sol);

  std::cout << std::fixed << std::setprecision(3);

  std::cout << "Rutas:\n";
  for (size_t i = 0; i < evaluated.routes.size(); ++i) {
    std::cout << "  Ruta " << i + 1 << ": ";
    for (size_t j = 0; j < evaluated.routes[i].size(); ++j) {
      std::cout << evaluated.routes[i][j];
      if (j + 1 < evaluated.routes[i].size())
        std::cout << " -> ";
    }
    std::cout << "\n";
  }

  std::cout << "Factible: " << (evaluated.feasible ? "si" : "no") << "\n";
  std::cout << "Distancia total (miles): " << evaluated.total_distance_miles
            << "\n";
  std::cout << "Energia total (kWh): " << evaluated.total_energy_kwh << "\n";
  std::cout << "Emisiones (kg CO2): " << evaluated.total_emissions_kg << "\n";

  std::unordered_map<int, int> customer_visit_count;
  for (const auto &route : evaluated.routes) {
    for (int node : route) {
      if (is_customer(inst, node)) {
        customer_visit_count[node]++;
      }
    }
  }

  std::vector<int> missing_customers;
  std::vector<int> repeated_customers;

  for (int c : inst.customer_ids) {
    int count = customer_visit_count[c];
    if (count == 0)
      missing_customers.push_back(c);
    if (count > 1)
      repeated_customers.push_back(c);
  }

  std::cout << "Clientes visitados: "
            << (inst.customer_ids.size() - missing_customers.size()) << "/"
            << inst.customer_ids.size() << "\n";

  if (!missing_customers.empty()) {
    std::cout << "Clientes faltantes:\n";
    for (int c : missing_customers) {
      std::cout << "  - " << node_label(inst, c) << "\n";
    }
  }

  if (!repeated_customers.empty()) {
    std::cout << "Clientes repetidos:\n";
    for (int c : repeated_customers) {
      std::cout << "  - " << node_label(inst, c)
                << " (visitas=" << customer_visit_count[c] << ")\n";
    }
  }

  if (!evaluated.feasible) {
    std::cout << "Error: " << evaluated.error_message << "\n";
  }

  std::cout << "\n========== DETALLE POR RUTA ==========\n";
  for (size_t i = 0; i < evaluated.routes.size(); ++i) {
    print_route_summary(inst, evaluated.routes[i], (int)i);
    std::cout << "\n";
  }
}
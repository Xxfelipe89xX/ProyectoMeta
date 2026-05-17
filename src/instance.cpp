#include "../include/instance.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

static std::string trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

static bool starts_with(const std::string &s, const std::string &prefix) {
  return s.rfind(prefix, 0) == 0;
}

RawInstance read_raw_instance(const std::string &filename) {
  std::ifstream in(filename);
  if (!in.is_open()) {
    throw std::runtime_error("No se pudo abrir el archivo: " + filename);
  }

  RawInstance raw;
  std::string line;

  enum class Section { HEADER, NODE_COORD, DISTANCE, SPEED };

  Section section = Section::HEADER;
  int distance_rows_read = 0;
  int speed_rows_read = 0;

  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty() || starts_with(line, "#"))
      continue;
    if (line == "EOF")
      break;

    if (line == "NODE_COORD_SECTION") {
      section = Section::NODE_COORD;
      continue;
    }
    if (line == "DISTANCE_MATRIX_MILES") {
      section = Section::DISTANCE;
      raw.distance_miles.assign(raw.total_nodes,
                                std::vector<double>(raw.total_nodes, 0.0));
      continue;
    }
    if (line == "SPEED_MATRIX_KMH") {
      section = Section::SPEED;
      raw.speed_kmh.assign(raw.total_nodes,
                           std::vector<double>(raw.total_nodes, 0.0));
      continue;
    }

    if (section == Section::HEADER) {
      auto pos = line.find(':');
      if (pos == std::string::npos)
        continue;

      std::string key = trim(line.substr(0, pos));
      std::string value = trim(line.substr(pos + 1));

      if (key == "NAME")
        raw.name = value;
      else if (key == "CUSTOMERS")
        raw.n_customers = std::stoi(value);
      else if (key == "STATIONS")
        raw.n_stations = std::stoi(value);
      else if (key == "TOTAL_NODES")
        raw.total_nodes = std::stoi(value);
      else if (key == "VEHICLE_CAPACITY_TONS")
        raw.vehicle_capacity_tons = std::stod(value);
      else if (key == "BATTERY_CAPACITY_KWH")
        raw.battery_capacity_kwh = std::stod(value);
      else if (key == "CURB_WEIGHT_KG")
        raw.curb_weight_kg = std::stod(value);
      else if (key == "ALPHA")
        raw.alpha = std::stod(value);
      else if (key == "BETA")
        raw.beta = std::stod(value);
      else if (key == "EFF_MOTOR")
        raw.eff_motor = std::stod(value);
      else if (key == "EFF_BATTERY_DISCHARGE")
        raw.eff_battery_discharge = std::stod(value);
      else if (key == "EFF_RECHARGE")
        raw.eff_recharge = std::stod(value);
      else if (key == "EMISSION_RATE_KG_PER_KWH")
        raw.emission_rate_kg_per_kwh = std::stod(value);
      else if (key == "MAX_VEHICLES")
        raw.max_vehicles = std::stoi(value);
      else if (key == "GRID_SIZE_MILES")
        raw.grid_size_miles = std::stod(value);
      else if (key == "MILES_TO_METERS")
        raw.miles_to_meters = std::stod(value);
    } else if (section == Section::NODE_COORD) {
      std::istringstream iss(line);
      int id;
      std::string type_s;
      double x, y, demand;
      iss >> id >> type_s >> x >> y >> demand;

      NodeType type;
      if (type_s == "D")
        type = NodeType::DEPOT_START;
      else if (type_s == "C")
        type = NodeType::CUSTOMER;
      else
        type = NodeType::STATION;

      raw.base_nodes.push_back({id, id, type, x, y, demand});
    } else if (section == Section::DISTANCE) {
      std::istringstream iss(line);
      for (int j = 0; j < raw.total_nodes; ++j) {
        iss >> raw.distance_miles[distance_rows_read][j];
      }
      distance_rows_read++;
    } else if (section == Section::SPEED) {
      std::istringstream iss(line);
      for (int j = 0; j < raw.total_nodes; ++j) {
        iss >> raw.speed_kmh[speed_rows_read][j];
      }
      speed_rows_read++;
    }
  }

  if ((int)raw.base_nodes.size() != raw.total_nodes) {
    throw std::runtime_error("NODE_COORD_SECTION incompleta.");
  }
  if (distance_rows_read != raw.total_nodes) {
    throw std::runtime_error("DISTANCE_MATRIX_MILES incompleta.");
  }
  if (speed_rows_read != raw.total_nodes) {
    throw std::runtime_error("SPEED_MATRIX_KMH incompleta.");
  }

  return raw;
}

Instance build_internal_instance(const RawInstance &raw) {
  Instance inst;
  inst.name = raw.name;
  inst.vehicle_capacity_tons = raw.vehicle_capacity_tons;
  inst.battery_capacity_kwh = raw.battery_capacity_kwh;
  inst.curb_weight_kg = raw.curb_weight_kg;
  inst.alpha = raw.alpha;
  inst.beta = raw.beta;
  inst.eff_motor = raw.eff_motor;
  inst.eff_battery_discharge = raw.eff_battery_discharge;
  inst.eff_recharge = raw.eff_recharge;
  inst.emission_rate_kg_per_kwh = raw.emission_rate_kg_per_kwh;
  inst.max_vehicles = raw.max_vehicles;
  inst.miles_to_meters = raw.miles_to_meters;

  const Node &depot_raw = raw.base_nodes[0];

  inst.nodes.push_back(
      {0, depot_raw.id, NodeType::DEPOT_START, depot_raw.x, depot_raw.y, 0.0});
  inst.nodes.push_back(
      {1, depot_raw.id, NodeType::DEPOT_END, depot_raw.x, depot_raw.y, 0.0});

  for (size_t i = 1; i < raw.base_nodes.size(); ++i) {
    const Node &b = raw.base_nodes[i];
    int new_id = (int)inst.nodes.size();

    NodeType new_type =
        b.type == NodeType::CUSTOMER ? NodeType::CUSTOMER : NodeType::STATION;
    inst.nodes.push_back({new_id, b.id, new_type, b.x, b.y, b.demand});

    if (new_type == NodeType::CUSTOMER)
      inst.customer_ids.push_back(new_id);
    else
      inst.station_ids.push_back(new_id);
  }

  int n = (int)inst.nodes.size();
  inst.distance_miles.assign(n, std::vector<double>(n, 0.0));
  inst.speed_kmh.assign(n, std::vector<double>(n, 0.0));

  auto map_internal_to_raw = [&](int internal_id) {
    return inst.nodes[internal_id].original_id;
  };

  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      int ri = map_internal_to_raw(i);
      int rj = map_internal_to_raw(j);

      if (i == j) {
        inst.distance_miles[i][j] = 0.0;
        inst.speed_kmh[i][j] = 0.0;
      } else {
        inst.distance_miles[i][j] = raw.distance_miles[ri][rj];
        inst.speed_kmh[i][j] = raw.speed_kmh[ri][rj];
      }
    }
  }

  return inst;
}

void print_instance_summary(const Instance &inst) {
  std::cout << "Instancia: " << inst.name << "\n";
  std::cout << "Nodos internos: " << inst.nodes.size() << "\n";
  std::cout << "Clientes: " << inst.customer_ids.size() << "\n";
  std::cout << "Estaciones: " << inst.station_ids.size() << "\n";
  std::cout << "Capacidad vehiculo (ton): " << inst.vehicle_capacity_tons
            << "\n";
  std::cout << "Capacidad bateria (kWh): " << inst.battery_capacity_kwh << "\n";
  std::cout << "Max vehiculos: " << inst.max_vehicles << "\n";
}

bool is_customer(const Instance &inst, int node_id) {
  return inst.nodes[node_id].type == NodeType::CUSTOMER;
}

bool is_station(const Instance &inst, int node_id) {
  return inst.nodes[node_id].type == NodeType::STATION;
}

bool is_depot_start(const Instance &inst, int node_id) {
  return inst.nodes[node_id].type == NodeType::DEPOT_START;
}

bool is_depot_end(const Instance &inst, int node_id) {
  return inst.nodes[node_id].type == NodeType::DEPOT_END;
}
#ifndef INSTANCE_HPP
#define INSTANCE_HPP

#include <string>
#include <vector>

enum class NodeType { DEPOT_START, DEPOT_END, CUSTOMER, STATION };

struct Node {
  int id;
  int original_id;
  NodeType type;
  double x;
  double y;
  double demand;
};

struct RawInstance {
  std::string name;

  int n_customers = 0;
  int n_stations = 0;
  int total_nodes = 0;

  double vehicle_capacity_tons = 0.0;
  double battery_capacity_kwh = 0.0;
  double curb_weight_kg = 0.0;
  double alpha = 0.0;
  double beta = 0.0;
  double eff_motor = 0.0;
  double eff_battery_discharge = 0.0;
  double eff_recharge = 0.0;
  double emission_rate_kg_per_kwh = 0.0;
  int max_vehicles = 0;
  double grid_size_miles = 0.0;
  double miles_to_meters = 1609.34;

  std::vector<Node> base_nodes;
  std::vector<std::vector<double>> distance_miles;
  std::vector<std::vector<double>> speed_kmh;
};

struct Instance {
  std::string name;

  double vehicle_capacity_tons = 0.0;
  double battery_capacity_kwh = 0.0;
  double curb_weight_kg = 0.0;
  double alpha = 0.0;
  double beta = 0.0;
  double eff_motor = 0.0;
  double eff_battery_discharge = 0.0;
  double eff_recharge = 0.0;
  double emission_rate_kg_per_kwh = 0.0;
  int max_vehicles = 0;
  double miles_to_meters = 1609.34;

  std::vector<Node> nodes;
  std::vector<std::vector<double>> distance_miles;
  std::vector<std::vector<double>> speed_kmh;

  int depot_start = 0;
  int depot_end = 1;

  std::vector<int> customer_ids;
  std::vector<int> station_ids;
};

RawInstance read_raw_instance(const std::string &filename);
Instance build_internal_instance(const RawInstance &raw);
void print_instance_summary(const Instance &inst);

bool is_customer(const Instance &inst, int node_id);
bool is_station(const Instance &inst, int node_id);
bool is_depot_start(const Instance &inst, int node_id);
bool is_depot_end(const Instance &inst, int node_id);

#endif
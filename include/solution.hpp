#ifndef SOLUTION_HPP
#define SOLUTION_HPP

#include "instance.hpp"
#include <string>
#include <vector>

struct ArcDetail {
  int from = -1;
  int to = -1;

  double distance_miles = 0.0;
  double speed_kmh = 0.0;

  double load_before_tons = 0.0;
  double load_after_tons = 0.0;

  double battery_before_kwh = 0.0;
  double energy_used_kwh = 0.0;
  double battery_after_kwh = 0.0;

  double emissions_kg = 0.0;

  bool recharge_before_arc = false;
  bool feasible = true;
  std::string error_message;
};

struct RouteEvaluation {
  bool feasible = true;
  double total_demand_tons = 0.0;
  double total_distance_miles = 0.0;
  double total_energy_kwh = 0.0;
  double total_emissions_kg = 0.0;
  std::string error_message;

  std::vector<ArcDetail> arcs;
};

struct Solution {
  std::vector<std::vector<int>> routes;
  double total_energy_kwh = 0.0;
  double total_emissions_kg = 0.0;
  double total_distance_miles = 0.0;
  bool feasible = true;
  std::string error_message;
};

double route_total_demand(const Instance &inst, const std::vector<int> &route);
RouteEvaluation evaluate_route(const Instance &inst,
                               const std::vector<int> &route);
Solution evaluate_solution(const Instance &inst, const Solution &sol);
void print_solution(const Instance &inst, const Solution &sol);

#endif
#include "../include/constructive.hpp"
#include "../include/solution.hpp"

#include <algorithm>
#include <limits>
#include <unordered_set>
#include <vector>

struct CandidateMove {
  bool feasible = false;
  int customer = -1;
  std::vector<int>
      append_nodes; // nodos a agregar a la ruta abierta (sin depot_end)
  double total_energy_kwh = std::numeric_limits<double>::max();
  double total_distance_miles = std::numeric_limits<double>::max();
  int added_stations = 0;
};

static double route_distance_miles(const Instance &inst,
                                   const std::vector<int> &route) {
  double total = 0.0;
  for (size_t i = 0; i + 1 < route.size(); ++i) {
    total += inst.distance_miles[route[i]][route[i + 1]];
  }
  return total;
}

static bool lexicographically_smaller(const std::vector<int> &a,
                                      const std::vector<int> &b) {
  return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
}

static bool better_candidate(const CandidateMove &a, const CandidateMove &b) {
  const double EPS = 1e-9;

  if (!a.feasible)
    return false;
  if (!b.feasible)
    return true;

  if (a.total_energy_kwh + EPS < b.total_energy_kwh)
    return true;
  if (b.total_energy_kwh + EPS < a.total_energy_kwh)
    return false;

  if (a.added_stations < b.added_stations)
    return true;
  if (b.added_stations < a.added_stations)
    return false;

  if (a.total_distance_miles + EPS < b.total_distance_miles)
    return true;
  if (b.total_distance_miles + EPS < a.total_distance_miles)
    return false;

  if (a.customer < b.customer)
    return true;
  if (b.customer < a.customer)
    return false;

  return lexicographically_smaller(a.append_nodes, b.append_nodes);
}

static CandidateMove try_pattern(const Instance &inst,
                                 const std::vector<int> &open_route,
                                 int customer, int station_before,
                                 int station_after) {
  CandidateMove cand;

  int last = open_route.back();

  if (station_before == last)
    return cand;

  std::vector<int> tentative = open_route;

  int stations_used = 0;

  if (station_before != -1) {
    tentative.push_back(station_before);
    stations_used++;
  }

  tentative.push_back(customer);

  if (station_after != -1) {
    if (station_after == tentative.back())
      return cand;
    tentative.push_back(station_after);
    stations_used++;
  }

  tentative.push_back(inst.depot_end);

  RouteEvaluation eval = evaluate_route(inst, tentative);
  if (!eval.feasible)
    return cand;

  cand.feasible = true;
  cand.customer = customer;
  cand.total_energy_kwh = eval.total_energy_kwh;
  cand.total_distance_miles = route_distance_miles(inst, tentative);
  cand.added_stations = stations_used;

  for (size_t i = open_route.size(); i + 1 < tentative.size(); ++i) {
    cand.append_nodes.push_back(tentative[i]);
  }

  return cand;
}

static CandidateMove
best_append_for_customer(const Instance &inst,
                         const std::vector<int> &open_route, int customer) {
  CandidateMove best;

  CandidateMove c0 = try_pattern(inst, open_route, customer, -1, -1);
  if (better_candidate(c0, best))
    best = c0;

  for (int s_after : inst.station_ids) {
    CandidateMove c1 = try_pattern(inst, open_route, customer, -1, s_after);
    if (better_candidate(c1, best))
      best = c1;
  }

  for (int s_before : inst.station_ids) {
    CandidateMove c2 = try_pattern(inst, open_route, customer, s_before, -1);
    if (better_candidate(c2, best))
      best = c2;
  }

  for (int s_before : inst.station_ids) {
    for (int s_after : inst.station_ids) {
      CandidateMove c3 =
          try_pattern(inst, open_route, customer, s_before, s_after);
      if (better_candidate(c3, best))
        best = c3;
    }
  }

  return best;
}

static CandidateMove best_next_move(const Instance &inst,
                                    const std::vector<int> &open_route,
                                    const std::unordered_set<int> &visited) {
  CandidateMove best;

  for (int c : inst.customer_ids) {
    if (visited.count(c))
      continue;

    CandidateMove cand = best_append_for_customer(inst, open_route, c);
    if (better_candidate(cand, best)) {
      best = cand;
    }
  }

  return best;
}

Solution greedy_constructive(const Instance &inst) {
  Solution sol;
  std::unordered_set<int> visited;

  while ((int)visited.size() < (int)inst.customer_ids.size() &&
         (int)sol.routes.size() < inst.max_vehicles) {

    std::vector<int> open_route;
    open_route.push_back(inst.depot_start);

    bool added_customer = false;

    while (true) {
      CandidateMove best = best_next_move(inst, open_route, visited);
      if (!best.feasible)
        break;

      for (int node : best.append_nodes) {
        open_route.push_back(node);
      }

      visited.insert(best.customer);
      added_customer = true;
    }

    if (!added_customer) {
      break;
    }

    open_route.push_back(inst.depot_end);
    sol.routes.push_back(open_route);
  }

  sol = evaluate_solution(inst, sol);
  return sol;
}
#include "../include/repair.hpp"
#include "../include/solution.hpp"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <vector>

struct RepairCandidate {
  bool feasible = false;
  bool create_new_route = false;
  int route_idx = -1;
  int insert_pos = -1;
  int customer = -1;
  std::vector<int> insertion;
  double delta_energy = std::numeric_limits<double>::max();
};

struct RelocateCandidate {
  bool feasible = false;
  int from_route = -1;
  int to_route = -1;
  int from_pos = -1;
  int to_pos = -1;
  double delta_energy = std::numeric_limits<double>::max();
};

static std::vector<int> get_missing_customers(const Instance &inst,
                                              const Solution &sol) {
  std::unordered_map<int, int> count;
  for (const auto &route : sol.routes) {
    for (int node : route) {
      if (is_customer(inst, node))
        count[node]++;
    }
  }

  std::vector<int> missing;
  for (int c : inst.customer_ids) {
    if (count[c] == 0)
      missing.push_back(c);
  }
  return missing;
}

static std::vector<int>
build_inserted_route(const std::vector<int> &route, int pos,
                     const std::vector<int> &insertion) {
  std::vector<int> out;
  out.reserve(route.size() + insertion.size());

  for (int i = 0; i < pos; ++i)
    out.push_back(route[i]);
  for (int x : insertion)
    out.push_back(x);
  for (size_t i = pos; i < route.size(); ++i)
    out.push_back(route[i]);

  return out;
}

static std::vector<int> nearest_stations_to_customer(const Instance &inst,
                                                     int customer, int k) {
  std::vector<std::pair<double, int>> cand;
  cand.reserve(inst.station_ids.size());

  for (int s : inst.station_ids) {
    cand.push_back({inst.distance_miles[customer][s], s});
  }

  std::sort(cand.begin(), cand.end());

  std::vector<int> result;
  for (int i = 0; i < (int)cand.size() && i < k; ++i) {
    result.push_back(cand[i].second);
  }
  return result;
}

static RepairCandidate
try_existing_route_pattern(const Instance &inst, const std::vector<int> &route,
                           double old_energy, int route_idx, int pos,
                           int customer, int station_before,
                           int station_after) {
  RepairCandidate cand;

  std::vector<int> insertion;
  if (station_before != -1)
    insertion.push_back(station_before);
  insertion.push_back(customer);
  if (station_after != -1)
    insertion.push_back(station_after);

  std::vector<int> tentative = build_inserted_route(route, pos, insertion);
  RouteEvaluation re = evaluate_route(inst, tentative);
  if (!re.feasible)
    return cand;

  cand.feasible = true;
  cand.create_new_route = false;
  cand.route_idx = route_idx;
  cand.insert_pos = pos;
  cand.customer = customer;
  cand.insertion = insertion;
  cand.delta_energy = re.total_energy_kwh - old_energy;

  return cand;
}

static RepairCandidate best_existing_route_insertion(const Instance &inst,
                                                     const Solution &sol,
                                                     int customer,
                                                     int num_near_stations) {
  RepairCandidate best;
  std::vector<int> near_stations =
      nearest_stations_to_customer(inst, customer, num_near_stations);

  for (size_t r = 0; r < sol.routes.size(); ++r) {
    const auto &route = sol.routes[r];
    RouteEvaluation old_re = evaluate_route(inst, route);
    if (!old_re.feasible)
      continue;

    for (int pos = 1; pos < (int)route.size(); ++pos) {
      RepairCandidate c0 = try_existing_route_pattern(
          inst, route, old_re.total_energy_kwh, (int)r, pos, customer, -1, -1);
      if (c0.feasible && c0.delta_energy < best.delta_energy)
        best = c0;

      for (int s : near_stations) {
        RepairCandidate c1 = try_existing_route_pattern(
            inst, route, old_re.total_energy_kwh, (int)r, pos, customer, s, -1);
        if (c1.feasible && c1.delta_energy < best.delta_energy)
          best = c1;

        RepairCandidate c2 = try_existing_route_pattern(
            inst, route, old_re.total_energy_kwh, (int)r, pos, customer, -1, s);
        if (c2.feasible && c2.delta_energy < best.delta_energy)
          best = c2;
      }

      for (int sb : near_stations) {
        for (int sa : near_stations) {
          RepairCandidate c3 =
              try_existing_route_pattern(inst, route, old_re.total_energy_kwh,
                                         (int)r, pos, customer, sb, sa);
          if (c3.feasible && c3.delta_energy < best.delta_energy)
            best = c3;
        }
      }
    }
  }

  return best;
}

static RepairCandidate best_new_route_insertion(const Instance &inst,
                                                const Solution &sol,
                                                int customer,
                                                int num_near_stations) {
  RepairCandidate best;
  if ((int)sol.routes.size() >= inst.max_vehicles)
    return best;

  std::vector<int> near_stations =
      nearest_stations_to_customer(inst, customer, num_near_stations);

  auto try_pattern = [&](int sb, int sa) {
    std::vector<int> route;
    route.push_back(inst.depot_start);
    if (sb != -1)
      route.push_back(sb);
    route.push_back(customer);
    if (sa != -1)
      route.push_back(sa);
    route.push_back(inst.depot_end);

    RouteEvaluation re = evaluate_route(inst, route);
    if (!re.feasible)
      return;

    std::vector<int> insertion;
    if (sb != -1)
      insertion.push_back(sb);
    insertion.push_back(customer);
    if (sa != -1)
      insertion.push_back(sa);

    if (!best.feasible || re.total_energy_kwh < best.delta_energy) {
      best.feasible = true;
      best.create_new_route = true;
      best.customer = customer;
      best.insertion = insertion;
      best.delta_energy = re.total_energy_kwh;
    }
  };

  try_pattern(-1, -1);

  for (int s : near_stations) {
    try_pattern(s, -1);
    try_pattern(-1, s);
  }

  for (int sb : near_stations) {
    for (int sa : near_stations) {
      try_pattern(sb, sa);
    }
  }

  return best;
}

static bool apply_repair_candidate(const Instance &inst, Solution &sol,
                                   const RepairCandidate &cand) {
  if (!cand.feasible)
    return false;

  if (cand.create_new_route) {
    std::vector<int> route;
    route.push_back(inst.depot_start);
    for (int x : cand.insertion)
      route.push_back(x);
    route.push_back(inst.depot_end);
    sol.routes.push_back(route);
  } else {
    sol.routes[cand.route_idx] = build_inserted_route(
        sol.routes[cand.route_idx], cand.insert_pos, cand.insertion);
  }

  sol = evaluate_solution(inst, sol);
  return true;
}

static bool run_repair_phase(const Instance &inst, Solution &sol,
                             int num_near_stations) {
  bool changed = false;

  for (int iter = 0; iter < (int)inst.customer_ids.size(); ++iter) {
    std::vector<int> missing = get_missing_customers(inst, sol);
    if (missing.empty())
      break;

    RepairCandidate best_overall;

    for (int c : missing) {
      RepairCandidate best_existing =
          best_existing_route_insertion(inst, sol, c, num_near_stations);
      if (best_existing.feasible &&
          best_existing.delta_energy < best_overall.delta_energy) {
        best_overall = best_existing;
      }

      RepairCandidate best_new =
          best_new_route_insertion(inst, sol, c, num_near_stations);
      if (best_new.feasible &&
          best_new.delta_energy < best_overall.delta_energy) {
        best_overall = best_new;
      }
    }

    if (!best_overall.feasible)
      break;

    if (!apply_repair_candidate(inst, sol, best_overall))
      break;
    changed = true;
  }

  return changed;
}

static RelocateCandidate best_single_relocate(const Instance &inst,
                                              const Solution &sol) {
  RelocateCandidate best;
  const double EPS = 1e-9;

  for (int a = 0; a < (int)sol.routes.size(); ++a) {
    RouteEvaluation old_a = evaluate_route(inst, sol.routes[a]);
    if (!old_a.feasible)
      continue;

    for (int i = 1; i < (int)sol.routes[a].size() - 1; ++i) {
      int node = sol.routes[a][i];
      if (!is_customer(inst, node))
        continue;

      std::vector<int> source = sol.routes[a];
      source.erase(source.begin() + i);
      RouteEvaluation new_a = evaluate_route(inst, source);
      if (!new_a.feasible)
        continue;

      for (int b = 0; b < (int)sol.routes.size(); ++b) {
        if (a == b)
          continue;

        RouteEvaluation old_b = evaluate_route(inst, sol.routes[b]);
        if (!old_b.feasible)
          continue;

        for (int j = 1; j < (int)sol.routes[b].size(); ++j) {
          std::vector<int> target = sol.routes[b];
          target.insert(target.begin() + j, node);

          RouteEvaluation new_b = evaluate_route(inst, target);
          if (!new_b.feasible)
            continue;

          double old_total = old_a.total_energy_kwh + old_b.total_energy_kwh;
          double new_total = new_a.total_energy_kwh + new_b.total_energy_kwh;
          double delta = new_total - old_total;

          if (!best.feasible || delta + EPS < best.delta_energy) {
            best.feasible = true;
            best.from_route = a;
            best.to_route = b;
            best.from_pos = i;
            best.to_pos = j;
            best.delta_energy = delta;
          }
        }
      }
    }
  }

  return best;
}

static bool apply_single_relocate(const Instance &inst, Solution &sol,
                                  const RelocateCandidate &move) {
  if (!move.feasible)
    return false;

  int node = sol.routes[move.from_route][move.from_pos];
  sol.routes[move.from_route].erase(sol.routes[move.from_route].begin() +
                                    move.from_pos);
  sol.routes[move.to_route].insert(
      sol.routes[move.to_route].begin() + move.to_pos, node);

  std::vector<std::vector<int>> cleaned;
  for (const auto &route : sol.routes) {
    if (route.size() > 2)
      cleaned.push_back(route);
    else {
      RouteEvaluation re = evaluate_route(inst, route);
      if (re.feasible && route.size() == 2) {
        continue;
      }
    }
  }
  sol.routes = cleaned;

  sol = evaluate_solution(inst, sol);
  return true;
}

Solution repair_missing_customers(const Instance &inst,
                                  const Solution &initial) {
  Solution sol = initial;
  sol = evaluate_solution(inst, sol);

  run_repair_phase(inst, sol, 1);

  if (get_missing_customers(inst, sol).empty()) {
    return evaluate_solution(inst, sol);
  }

  run_repair_phase(inst, sol, 2);

  if (get_missing_customers(inst, sol).empty()) {
    return evaluate_solution(inst, sol);
  }

  for (int attempt = 0; attempt < 3; ++attempt) {
    RelocateCandidate move = best_single_relocate(inst, sol);
    if (!move.feasible)
      break;

    Solution moved = sol;
    if (!apply_single_relocate(inst, moved, move))
      break;

    run_repair_phase(inst, moved, 2);

    std::vector<int> missing_after = get_missing_customers(inst, moved);
    std::vector<int> missing_before = get_missing_customers(inst, sol);

    if (missing_after.size() < missing_before.size()) {
      sol = moved;
    } else {
      break;
    }

    if (missing_after.empty())
      break;
  }

  return evaluate_solution(inst, sol);
}
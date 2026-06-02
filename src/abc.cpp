#include "../include/abc.hpp"
#include "../include/energy.hpp"
#include "../include/instance.hpp"
#include "../include/multi_greedy.hpp"
#include "../include/solution.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <unordered_map>
#include <vector>


// Helper to compute loads carried on each arc of a route
static std::vector<double> get_arc_loads(const Instance &inst, const std::vector<int> &route) {
  std::vector<double> loads(route.size(), 0.0);
  double remaining = 0.0;
  for (int node : route) {
    if (is_customer(inst, node)) {
      remaining += inst.nodes[node].demand;
    }
  }
  for (size_t i = 0; i < route.size(); ++i) {
    loads[i] = remaining;
    if (i + 1 < route.size()) {
      int v = route[i + 1];
      if (is_customer(inst, v)) {
        remaining -= inst.nodes[v].demand;
      }
    }
  }
  return loads;
}

// Recharging Station Insertion using Minimum Detour (MBD)
static std::vector<int> insert_stations_mbd(const Instance &inst, const std::vector<int> &customer_route) {
  std::vector<int> route;
  route.push_back(inst.depot_start);
  route.insert(route.end(), customer_route.begin(), customer_route.end());
  route.push_back(inst.depot_end);

  if (inst.station_ids.empty()) {
    return route;
  }

  int max_insertions = 20;
  for (int iter = 0; iter < max_insertions; ++iter) {
    std::vector<double> loads = get_arc_loads(inst, route);

    // Evaluate battery levels along the route
    std::vector<double> battery(route.size(), 0.0);
    battery[0] = inst.battery_capacity_kwh;
    
    int violation_idx = -1;
    for (size_t i = 0; i + 1 < route.size(); ++i) {
      int u = route[i];
      int v = route[i + 1];

      if (is_depot_start(inst, u) || is_station(inst, u)) {
        battery[i] = inst.battery_capacity_kwh;
      }

      double energy = arc_energy_kwh(inst, u, v, loads[i]);
      if (energy > battery[i] + 1e-9) {
        violation_idx = i + 1; // v is unreachable
        break;
      }
      battery[i + 1] = battery[i] - energy;
    }

    if (violation_idx == -1) {
      // No battery violation
      break;
    }

    // Find the last recharging location index before the violation index
    int last_recharge_idx = 0;
    for (int k = violation_idx - 1; k >= 0; --k) {
      if (is_depot_start(inst, route[k]) || is_station(inst, route[k])) {
        last_recharge_idx = k;
        break;
      }
    }

    bool inserted = false;
    // Try inserting a station after route[p], where p goes from violation_idx - 1 down to last_recharge_idx + 1
    for (int p = violation_idx - 1; p > last_recharge_idx; --p) {
      int u = route[p];
      int w = route[p + 1];
      double load_at_u = loads[p];

      int best_s = -1;
      double best_detour = std::numeric_limits<double>::max();

      for (int s : inst.station_ids) {
        // Can we reach s from u?
        double energy_u_s = arc_energy_kwh(inst, u, s, load_at_u);
        if (energy_u_s > battery[p] + 1e-9) continue;

        // Can we reach w from s with full battery?
        double energy_s_w = arc_energy_kwh(inst, s, w, load_at_u);
        if (energy_s_w > inst.battery_capacity_kwh + 1e-9) continue;

        // Detour energy
        double detour = energy_u_s + energy_s_w - arc_energy_kwh(inst, u, w, load_at_u);
        if (detour < best_detour) {
          best_detour = detour;
          best_s = s;
        }
      }

      if (best_s != -1) {
        route.insert(route.begin() + p + 1, best_s);
        inserted = true;
        break;
      }
    }

    if (!inserted) {
      // Fallback: insert the best detour station at violation_idx - 1
      int p = violation_idx - 1;
      int u = route[p];
      int w = route[p + 1];
      double load_at_u = loads[p];

      int best_s = -1;
      double best_detour = std::numeric_limits<double>::max();
      for (int s : inst.station_ids) {
        double detour = arc_energy_kwh(inst, u, s, load_at_u) + arc_energy_kwh(inst, s, w, load_at_u) - arc_energy_kwh(inst, u, w, load_at_u);
        if (detour < best_detour) {
          best_detour = detour;
          best_s = s;
        }
      }

      if (best_s != -1) {
        route.insert(route.begin() + p + 1, best_s);
      } else {
        route.insert(route.begin() + p + 1, inst.station_ids.front());
      }
      break;
    }
  }

  return route;
}

// Remove all stations from the solution to obtain only customer sequences
static std::vector<std::vector<int>> strip_stations(const Instance &inst, const Solution &sol) {
  std::vector<std::vector<int>> stripped;
  for (const auto &route : sol.routes) {
    std::vector<int> customers;
    for (int node : route) {
      if (is_customer(inst, node)) {
        customers.push_back(node);
      }
    }
    if (!customers.empty()) {
      stripped.push_back(customers);
    }
  }
  return stripped;
}

// Rebuild full solution by inserting stations into each customer route
static Solution rebuild_solution(const Instance &inst, const std::vector<std::vector<int>> &customer_routes) {
  Solution sol;
  for (const auto &c_route : customer_routes) {
    sol.routes.push_back(insert_stations_mbd(inst, c_route));
  }
  return sol;
}

// Structure to hold penalized evaluation results
struct PenalizedEvaluation {
  double total_energy_kwh = 0.0;
  double capacity_violation_tons = 0.0;
  double battery_violation_kwh = 0.0;
  int missing_customers = 0;
  int repeated_customers = 0;
  bool feasible = true;

  double get_total_cost(double alpha, double beta) const {
    double cost = total_energy_kwh;
    cost += alpha * capacity_violation_tons;
    cost += beta * battery_violation_kwh;
    if (missing_customers > 0) {
      cost += missing_customers * 10000.0; // High static penalty
    }
    if (repeated_customers > 0) {
      cost += repeated_customers * 10000.0;
    }
    return cost;
  }
};

// Penalized evaluation function
static PenalizedEvaluation evaluate_solution_penalized(const Instance &inst, const Solution &sol) {
  PenalizedEvaluation eval;
  eval.feasible = true;

  if ((int)sol.routes.size() > inst.max_vehicles) {
    eval.feasible = false;
    eval.capacity_violation_tons += ((int)sol.routes.size() - inst.max_vehicles) * inst.vehicle_capacity_tons;
  }

  std::unordered_map<int, int> customer_visit_count;

  for (const auto &route : sol.routes) {
    if (route.size() < 2) continue;

    double demand = route_total_demand(inst, route);
    if (demand > inst.vehicle_capacity_tons) {
      eval.capacity_violation_tons += (demand - inst.vehicle_capacity_tons);
      eval.feasible = false;
    }

    std::vector<double> loads = get_arc_loads(inst, route);
    double current_battery = inst.battery_capacity_kwh;

    for (size_t i = 0; i + 1 < route.size(); ++i) {
      int u = route[i];
      int v = route[i + 1];

      if (is_depot_start(inst, u) || is_station(inst, u)) {
        current_battery = inst.battery_capacity_kwh;
      }

      double energy = arc_energy_kwh(inst, u, v, loads[i]);
      if (energy > current_battery + 1e-9) {
        eval.battery_violation_kwh += (energy - current_battery);
        eval.feasible = false;
        current_battery = 0;
      } else {
        current_battery -= energy;
      }

      eval.total_energy_kwh += energy;

      if (is_customer(inst, v)) {
        customer_visit_count[v]++;
      }
    }
  }

  for (int c : inst.customer_ids) {
    int count = customer_visit_count[c];
    if (count == 0) {
      eval.missing_customers++;
      eval.feasible = false;
    } else if (count > 1) {
      eval.repeated_customers += (count - 1);
      eval.feasible = false;
    }
  }

  return eval;
}

// Helper to generate a random customer-based solution
static Solution generate_random_abc_solution(const Instance &inst, std::mt19937 &rng) {
  std::vector<int> customers = inst.customer_ids;
  std::shuffle(customers.begin(), customers.end(), rng);

  std::vector<std::vector<int>> customer_routes;
  std::vector<int> current_route;
  double current_load = 0.0;

  for (int c : customers) {
    double demand = inst.nodes[c].demand;
    if (current_load + demand > inst.vehicle_capacity_tons) {
      if (!current_route.empty()) {
        customer_routes.push_back(current_route);
      }
      current_route.clear();
      current_load = 0.0;
    }
    current_route.push_back(c);
    current_load += demand;
  }
  if (!current_route.empty()) {
    customer_routes.push_back(current_route);
  }

  return rebuild_solution(inst, customer_routes);
}

// Neighborhood Operators applied on customer routes
static void apply_neighborhood_operator(std::vector<std::vector<int>> &customer_routes, int op_type, std::mt19937 &rng) {
  std::vector<int> flat;
  std::vector<size_t> route_sizes;
  for (const auto &r : customer_routes) {
    flat.insert(flat.end(), r.begin(), r.end());
    route_sizes.push_back(r.size());
  }

  if (flat.size() < 2) return;

  if (op_type == 0) {
    // 1. Random Swap
    std::uniform_int_distribution<size_t> dist(0, flat.size() - 1);
    size_t i = dist(rng);
    size_t j = dist(rng);
    while (i == j && flat.size() > 1) {
      j = dist(rng);
    }
    std::swap(flat[i], flat[j]);
  } else if (op_type == 1) {
    // 2. Reversing a subsequence
    std::uniform_int_distribution<size_t> dist(0, flat.size() - 1);
    size_t i = dist(rng);
    size_t j = dist(rng);
    if (i > j) std::swap(i, j);
    std::reverse(flat.begin() + i, flat.begin() + j + 1);
  } else {
    // 3. Random swaps of reversed subsequences
    if (flat.size() >= 4) {
      std::uniform_int_distribution<size_t> dist(0, flat.size() - 1);
      std::vector<size_t> pts;
      for (int k = 0; k < 4; ++k) {
        pts.push_back(dist(rng));
      }
      std::sort(pts.begin(), pts.end());
      size_t i1 = pts[0], j1 = pts[1], i2 = pts[2], j2 = pts[3];
      if (i1 != j1 && j1 < i2 && i2 != j2) {
        std::reverse(flat.begin() + i1, flat.begin() + j1 + 1);
        std::reverse(flat.begin() + i2, flat.begin() + j2 + 1);

        std::vector<int> next_flat;
        next_flat.reserve(flat.size());

        next_flat.insert(next_flat.end(), flat.begin(), flat.begin() + i1);
        next_flat.insert(next_flat.end(), flat.begin() + i2, flat.begin() + j2 + 1);
        next_flat.insert(next_flat.end(), flat.begin() + j1 + 1, flat.begin() + i2);
        next_flat.insert(next_flat.end(), flat.begin() + i1, flat.begin() + j1 + 1);
        next_flat.insert(next_flat.end(), flat.begin() + j2 + 1, flat.end());

        flat = next_flat;
      } else {
        std::uniform_int_distribution<size_t> fallback_dist(0, flat.size() - 1);
        size_t i = fallback_dist(rng);
        size_t j = fallback_dist(rng);
        if (i != j) std::swap(flat[i], flat[j]);
      }
    } else {
      std::uniform_int_distribution<size_t> fallback_dist(0, flat.size() - 1);
      size_t i = fallback_dist(rng);
      size_t j = fallback_dist(rng);
      if (i != j) std::swap(flat[i], flat[j]);
    }
  }

  // Restore back into customer_routes
  customer_routes.clear();
  size_t offset = 0;
  for (size_t size : route_sizes) {
    if (size == 0) continue;
    std::vector<int> r(flat.begin() + offset, flat.begin() + offset + size);
    customer_routes.push_back(r);
    offset += size;
  }
}

// Route Elimination Procedure
static void route_elimination(const Instance &inst, std::vector<std::vector<int>> &customer_routes, std::mt19937 &rng) {
  if (customer_routes.size() <= 1) return;

  double total_demand = 0.0;
  for (const auto &r : customer_routes) {
    for (int c : r) {
      total_demand += inst.nodes[c].demand;
    }
  }

  int M = customer_routes.size();
  int min_routes_needed = (int)std::ceil(total_demand / inst.vehicle_capacity_tons);
  int max_L = std::min(M - 1, M - min_routes_needed);
  if (max_L <= 0) return;

  std::uniform_int_distribution<int> dist_L(1, max_L);
  int L = dist_L(rng);

  std::vector<int> indices(M);
  std::iota(indices.begin(), indices.end(), 0);
  std::sort(indices.begin(), indices.end(), [&](int a, int b) {
    return customer_routes[a].size() < customer_routes[b].size();
  });

  std::vector<int> eliminated_indices(indices.begin(), indices.begin() + L);
  std::vector<int> remaining_indices(indices.begin() + L, indices.end());

  std::vector<int> customers_to_reinsert;
  for (int idx : eliminated_indices) {
    customers_to_reinsert.insert(customers_to_reinsert.end(), customer_routes[idx].begin(), customer_routes[idx].end());
  }

  std::shuffle(customers_to_reinsert.begin(), customers_to_reinsert.end(), rng);

  std::vector<std::vector<int>> new_routes;
  for (int idx : remaining_indices) {
    new_routes.push_back(customer_routes[idx]);
  }

  for (int c : customers_to_reinsert) {
    std::uniform_int_distribution<int> dist_route(0, new_routes.size() - 1);
    int r_idx = dist_route(rng);
    std::uniform_int_distribution<int> dist_pos(0, new_routes[r_idx].size());
    int pos = dist_pos(rng);
    new_routes[r_idx].insert(new_routes[r_idx].begin() + pos, c);
  }

  customer_routes = new_routes;
}

std::vector<Solution> load_solutions_from_json(const std::string &json_path) {
  std::vector<Solution> pool;
  std::ifstream in(json_path);
  if (!in.is_open()) {
    std::cerr << "[Warning] No se pudo abrir el archivo JSON: " << json_path << ". Se usara inicializacion multi-greedy al vuelo.\n";
    return pool;
  }

  std::string line;
  Solution current_sol;
  bool parsing_routes = false;

  while (std::getline(in, line)) {
    if (line.find("{") != std::string::npos) {
      current_sol = Solution{};
    }

    if (line.find("\"routes\": [") != std::string::npos) {
      parsing_routes = true;
      continue;
    }

    if (parsing_routes) {
      if (line.find("]") != std::string::npos && line.find("[") == std::string::npos) {
        parsing_routes = false;
        continue;
      }

      size_t start = line.find("[");
      size_t end = line.find("]");
      if (start != std::string::npos && end != std::string::npos && end > start) {
        std::string route_str = line.substr(start + 1, end - start - 1);
        std::vector<int> route;
        std::stringstream ss(route_str);
        std::string token;
        while (std::getline(ss, token, ',')) {
          if (!token.empty()) {
            try {
              route.push_back(std::stoi(token));
            } catch (...) {
            }
          }
        }
        if (!route.empty()) {
          current_sol.routes.push_back(route);
        }
      }
    }

    if (line.find("}") != std::string::npos) {
      if (!current_sol.routes.empty()) {
        pool.push_back(current_sol);
      }
    }
  }

  std::cerr << "[OK] Se cargaron " << pool.size() << " soluciones desde: " << json_path << "\n";
  return pool;
}

Solution run_artificial_bee_colony(const Instance &inst, const ABCConfig &config, const std::vector<Solution> &initial_pop) {
  std::mt19937 rng(config.seed);

  double alpha = 0.1;
  double beta = 0.1;
  const double theta = 0.001;

  int tau = config.population_size;
  std::vector<Solution> population;

  // Use the loaded solutions if available
  for (const auto &sol : initial_pop) {
    if (population.size() >= (size_t)tau) break;
    population.push_back(sol);
  }

  // If we need more solutions, use multi-greedy constructive to initialize
  if (population.size() < (size_t)tau) {
    MultiGreedyConfig mg_cfg;
    mg_cfg.num_solutions = tau - population.size();
    mg_cfg.top_k = 3;
    mg_cfg.seed = config.seed;
    mg_cfg.apply_repair = true;
    mg_cfg.apply_local_search = true;
    mg_cfg.local_search_iters = 5;
    mg_cfg.sort_output = false;

    std::vector<Solution> initial_pool = generate_greedy_solutions(inst, mg_cfg);
    for (const auto &sol : initial_pool) {
      if (population.size() >= (size_t)tau) break;
      population.push_back(sol);
    }
  }

  // If still not enough, generate random solutions
  while ((int)population.size() < tau) {
    population.push_back(generate_random_abc_solution(inst, rng));
  }

  // Evaluate initial fitness
  std::vector<PenalizedEvaluation> evaluations(tau);
  std::vector<double> fitness(tau);
  std::vector<int> trial_counters(tau, 0);

  for (int i = 0; i < tau; ++i) {
    evaluations[i] = evaluate_solution_penalized(inst, population[i]);
    double cost = evaluations[i].get_total_cost(alpha, beta);
    fitness[i] = (cost >= 0) ? (1.0 / (cost + 1e-9)) : (1.0 + std::abs(cost));
  }

  Solution best_feasible_sol;
  double best_feasible_energy = std::numeric_limits<double>::max();

  // Track initial best feasible solution
  for (int i = 0; i < tau; ++i) {
    if (evaluations[i].feasible) {
      Solution evaluated = evaluate_solution(inst, population[i]);
      if (evaluated.feasible && evaluated.total_energy_kwh < best_feasible_energy) {
        best_feasible_energy = evaluated.total_energy_kwh;
        best_feasible_sol = evaluated;
      }
    }
  }

  // Distribution for neighborhood operator selection (0: Swap, 1: Reverse, 2: Swap-Reversal)
  std::uniform_int_distribution<int> dist_operator(0, 2);
  std::uniform_real_distribution<double> dist_elim(0.0, 1.0);

  // Main ABC Loop
  for (int iter = 0; iter < config.max_iterations; ++iter) {
    // --- Phase 1: Employed Bees ---
    for (int i = 0; i < tau; ++i) {
      // Strip stations from current solution
      std::vector<std::vector<int>> c_routes = strip_stations(inst, population[i]);

      // Apply random neighborhood operator on customer routes
      int op = dist_operator(rng);
      apply_neighborhood_operator(c_routes, op, rng);

      // Route elimination
      if (dist_elim(rng) < config.route_elim_prob) {
        route_elimination(inst, c_routes, rng);
      }

      // Re-insert stations (MBD)
      Solution cand_sol = rebuild_solution(inst, c_routes);
      PenalizedEvaluation cand_eval = evaluate_solution_penalized(inst, cand_sol);

      double curr_cost = evaluations[i].get_total_cost(alpha, beta);
      double cand_cost = cand_eval.get_total_cost(alpha, beta);

      if (cand_cost < curr_cost) {
        population[i] = cand_sol;
        evaluations[i] = cand_eval;
        fitness[i] = (cand_cost >= 0) ? (1.0 / (cand_cost + 1e-9)) : (1.0 + std::abs(cand_cost));
        trial_counters[i] = 0;

        // Check feasibility
        if (cand_eval.feasible) {
          Solution evaluated = evaluate_solution(inst, cand_sol);
          if (evaluated.feasible && evaluated.total_energy_kwh < best_feasible_energy) {
            best_feasible_energy = evaluated.total_energy_kwh;
            best_feasible_sol = evaluated;
          }
        }
      } else {
        trial_counters[i]++;
      }
    }

    // --- Phase 2: Onlooker Bees ---
    // Calculate selection probabilities
    double sum_fitness = 0.0;
    for (double f : fitness) {
      sum_fitness += f;
    }

    std::vector<double> probs(tau, 0.0);
    if (sum_fitness > 1e-9) {
      for (int i = 0; i < tau; ++i) {
        probs[i] = fitness[i] / sum_fitness;
      }
    } else {
      for (int i = 0; i < tau; ++i) {
        probs[i] = 1.0 / tau;
      }
    }

    // Select and exploit
    for (int o = 0; o < tau; ++o) {
      // Roulette Wheel selection
      double r_val = dist_elim(rng);
      double accum = 0.0;
      int selected_idx = tau - 1;
      for (int i = 0; i < tau; ++i) {
        accum += probs[i];
        if (accum >= r_val) {
          selected_idx = i;
          break;
        }
      }

      // Mutate selected solution
      std::vector<std::vector<int>> c_routes = strip_stations(inst, population[selected_idx]);
      int op = dist_operator(rng);
      apply_neighborhood_operator(c_routes, op, rng);

      if (dist_elim(rng) < config.route_elim_prob) {
        route_elimination(inst, c_routes, rng);
      }

      Solution cand_sol = rebuild_solution(inst, c_routes);
      PenalizedEvaluation cand_eval = evaluate_solution_penalized(inst, cand_sol);

      double curr_cost = evaluations[selected_idx].get_total_cost(alpha, beta);
      double cand_cost = cand_eval.get_total_cost(alpha, beta);

      if (cand_cost < curr_cost) {
        population[selected_idx] = cand_sol;
        evaluations[selected_idx] = cand_eval;
        fitness[selected_idx] = (cand_cost >= 0) ? (1.0 / (cand_cost + 1e-9)) : (1.0 + std::abs(cand_cost));
        trial_counters[selected_idx] = 0;

        if (cand_eval.feasible) {
          Solution evaluated = evaluate_solution(inst, cand_sol);
          if (evaluated.feasible && evaluated.total_energy_kwh < best_feasible_energy) {
            best_feasible_energy = evaluated.total_energy_kwh;
            best_feasible_sol = evaluated;
          }
        }
      } else {
        trial_counters[selected_idx]++;
      }
    }

    // --- Phase 3: Scout Bees ---
    for (int i = 0; i < tau; ++i) {
      if (trial_counters[i] >= config.limit) {
        // Discard food source, generate a new random one
        population[i] = generate_random_abc_solution(inst, rng);
        evaluations[i] = evaluate_solution_penalized(inst, population[i]);
        double cost = evaluations[i].get_total_cost(alpha, beta);
        fitness[i] = (cost >= 0) ? (1.0 / (cost + 1e-9)) : (1.0 + std::abs(cost));
        trial_counters[i] = 0;

        if (evaluations[i].feasible) {
          Solution evaluated = evaluate_solution(inst, population[i]);
          if (evaluated.feasible && evaluated.total_energy_kwh < best_feasible_energy) {
            best_feasible_energy = evaluated.total_energy_kwh;
            best_feasible_sol = evaluated;
          }
        }
      }
    }

    // --- Phase 4: Dynamic Penalty Update ---
    int feasible_count = 0;
    for (int i = 0; i < tau; ++i) {
      if (evaluations[i].feasible) {
        feasible_count++;
      }
    }

    if (feasible_count > tau / 2) {
      alpha /= (1.0 + theta);
      beta /= (1.0 + theta);
    } else {
      alpha *= (1.0 + theta);
      beta *= (1.0 + theta);
    }
  }

  // Return the best feasible solution found. If none was feasible (unlikely), return the best overall from population.
  if (best_feasible_energy < std::numeric_limits<double>::max()) {
    return best_feasible_sol;
  }

  // Fallback to the best penalized solution in the current population
  int best_idx = 0;
  double best_cost = evaluations[0].get_total_cost(alpha, beta);
  for (int i = 1; i < tau; ++i) {
    double cost = evaluations[i].get_total_cost(alpha, beta);
    if (cost < best_cost) {
      best_cost = cost;
      best_idx = i;
    }
  }
  return evaluate_solution(inst, population[best_idx]);
}

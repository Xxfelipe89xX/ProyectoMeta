#include "../include/multi_greedy.hpp"
#include "../include/constructive.hpp"
#include "../include/local_search.hpp"
#include "../include/repair.hpp"
#include "../include/solution.hpp"

#include <algorithm>
#include <random>
#include <sstream>
#include <string>
#include <vector>

static std::string solution_signature(const Solution &sol) {
  std::ostringstream oss;
  for (const auto &route : sol.routes) {
    oss << "[";
    for (int node : route) {
      oss << node << ",";
    }
    oss << "]";
  }
  return oss.str();
}

static Solution postprocess_solution(const Instance &inst, Solution sol,
                                     const MultiGreedyConfig &config) {
  sol = evaluate_solution(inst, sol);

  if (config.apply_repair) {
    sol = repair_missing_customers(inst, sol);
  }

  if (config.apply_local_search) {
    sol = improve_by_local_search_light(inst, sol, config.local_search_iters);
  }

  sol = evaluate_solution(inst, sol);
  return sol;
}

std::vector<Solution>
generate_greedy_solutions(const Instance &inst,
                          const MultiGreedyConfig &config) {
  std::vector<Solution> pool;
  if (config.num_solutions <= 0)
    return pool;

  std::mt19937 rng(config.seed);
  
  std::vector<Solution> pool_feasible;
  std::vector<Solution> pool_infeasible;
  std::vector<std::string> seen_feasible;
  std::vector<std::string> seen_infeasible;

  auto push_feasible = [&](const Solution &sol) {
    std::string sig = solution_signature(sol);
    if (std::find(seen_feasible.begin(), seen_feasible.end(), sig) != seen_feasible.end())
      return false;
    seen_feasible.push_back(sig);
    pool_feasible.push_back(sol);
    return true;
  };

  auto push_infeasible = [&](const Solution &sol) {
    std::string sig = solution_signature(sol);
    if (std::find(seen_infeasible.begin(), seen_infeasible.end(), sig) != seen_infeasible.end())
      return false;
    seen_infeasible.push_back(sig);
    pool_infeasible.push_back(sol);
    return true;
  };

  int target_feasible = (int)(config.num_solutions * 0.7);
  if (target_feasible < 1 && config.num_solutions >= 2) {
    target_feasible = 1;
  }
  int target_infeasible = config.num_solutions - target_feasible;

  int max_attempts = std::max(config.num_solutions * 50, 200);
  int attempts = 0;

  while (attempts < max_attempts) {
    if ((int)pool_feasible.size() >= target_feasible && (int)pool_infeasible.size() >= target_infeasible) {
      break;
    }
    if (attempts >= 30 && (int)pool_feasible.size() + (int)pool_infeasible.size() >= config.num_solutions * 2) {
      break;
    }
    attempts++;

    Solution sol = greedy_constructive_randomized(inst, rng, config.top_k);
    
    Solution sol_feasible = postprocess_solution(inst, sol, config);
    if (sol_feasible.feasible) {
      push_feasible(sol_feasible);
    } else {
      push_infeasible(sol_feasible);
    }

    Solution sol_infeasible = evaluate_solution(inst, sol);
    if (!sol_infeasible.feasible) {
      push_infeasible(sol_infeasible);
    }
  }

  int take_feasible = std::min(target_feasible, (int)pool_feasible.size());
  for (int i = 0; i < take_feasible; ++i) {
    pool.push_back(pool_feasible[i]);
  }

  int take_infeasible = std::min(target_infeasible, (int)pool_infeasible.size());
  for (int i = 0; i < take_infeasible; ++i) {
    pool.push_back(pool_infeasible[i]);
  }

  while ((int)pool.size() < config.num_solutions && take_feasible < (int)pool_feasible.size()) {
    pool.push_back(pool_feasible[take_feasible++]);
  }
  while ((int)pool.size() < config.num_solutions && take_infeasible < (int)pool_infeasible.size()) {
    pool.push_back(pool_infeasible[take_infeasible++]);
  }

  if (pool.empty()) {
    Solution fallback = greedy_constructive(inst);
    fallback = postprocess_solution(inst, fallback, config);
    pool.push_back(fallback);
  }

  while ((int)pool.size() < config.num_solutions) {
    pool.push_back(pool.front());
  }

  if (config.sort_output) {
    std::sort(pool.begin(), pool.end(),
              [](const Solution &a, const Solution &b) {
                if (a.feasible != b.feasible)
                  return a.feasible > b.feasible;
                return a.total_energy_kwh < b.total_energy_kwh;
              });
  }

  return pool;
}
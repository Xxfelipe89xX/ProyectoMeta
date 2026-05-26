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
  std::vector<std::string> seen;

  auto push_if_new = [&](const Solution &sol) {
    std::string sig = solution_signature(sol);
    if (std::find(seen.begin(), seen.end(), sig) != seen.end())
      return false;
    seen.push_back(sig);
    pool.push_back(sol);
    return true;
  };

  int max_attempts = std::max(config.num_solutions * 10, 30);
  int attempts = 0;

  while ((int)pool.size() < config.num_solutions && attempts < max_attempts) {
    attempts++;

    Solution sol = greedy_constructive_randomized(inst, rng, config.top_k);
    sol = postprocess_solution(inst, sol, config);
    push_if_new(sol);
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
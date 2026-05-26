#ifndef MULTI_GREEDY_HPP
#define MULTI_GREEDY_HPP

#include "solution.hpp"
#include <vector>

struct MultiGreedyConfig {
  int num_solutions = 10;
  int top_k = 3;
  int seed = 12345;

  bool apply_repair = true;
  bool apply_local_search = false;
  int local_search_iters = 5;

  bool sort_output = true;
};

std::vector<Solution> generate_greedy_solutions(
    const Instance &inst,
    const MultiGreedyConfig &config = MultiGreedyConfig{});

#endif
#ifndef ABC_HPP
#define ABC_HPP

#include "instance.hpp"
#include "solution.hpp"
#include <string>
#include <vector>

struct ABCConfig {
  int population_size = 10;    
  int limit = 50;               
  int max_iterations = 3000;   
  int seed = 42;                
  double route_elim_prob = 0.5; 
};

std::vector<Solution> load_solutions_from_json(const std::string &json_path);

Solution run_artificial_bee_colony(const Instance &inst, const ABCConfig &config, const std::vector<Solution> &initial_pop);

#endif

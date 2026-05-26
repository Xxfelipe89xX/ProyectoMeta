#ifndef CONSTRUCTIVE_HPP
#define CONSTRUCTIVE_HPP

#include "instance.hpp"
#include "solution.hpp"

#include <random>

Solution greedy_constructive(const Instance &inst);

Solution greedy_constructive_randomized(const Instance &inst, std::mt19937 &rng,
                                        int top_k = 3);

#endif
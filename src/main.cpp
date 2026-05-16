#include "../include/constructive.hpp"
#include "../include/instance.hpp"
#include "../include/solution.hpp"
#include <iostream>


int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Uso: ./evrp data/C10R2.txt\n";
    return 1;
  }

  try {
    RawInstance raw = read_raw_instance(argv[1]);
    Instance inst = build_internal_instance(raw);

    print_instance_summary(inst);

    Solution sol = greedy_constructive(inst);
    print_solution(inst, sol);
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
#include "../include/constructive.hpp"
#include "../include/instance.hpp"
#include "../include/local_search.hpp"
#include "../include/repair.hpp"
#include "../include/solution.hpp"

#include <chrono>
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Uso: ./evrp instances/small/C10R2.txt\n";
    return 1;
  }

  try {
    auto t0 = std::chrono::steady_clock::now();

    RawInstance raw = read_raw_instance(argv[1]);
    Instance inst = build_internal_instance(raw);

    print_instance_summary(inst);

    auto t1 = std::chrono::steady_clock::now();
    std::cout << "[Etapa] Greedy constructive...\n";
    Solution sol = greedy_constructive(inst);
    auto t2 = std::chrono::steady_clock::now();

    std::cout << "[Etapa] Repair light...\n";
    sol = repair_missing_customers(inst, sol);
    auto t3 = std::chrono::steady_clock::now();

    std::cout << "[Etapa] Local search light...\n";
    sol = improve_by_local_search_light(inst, sol, 10);
    auto t4 = std::chrono::steady_clock::now();

    std::cout << "[Etapa] Resultado final\n";
    print_solution(inst, sol);

    auto ms_read =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    auto ms_greedy =
        std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    auto ms_repair =
        std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
    auto ms_ls =
        std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count();
    auto ms_total =
        std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t0).count();

    std::cout << "\nTiempos (ms):\n";
    std::cout << "  Lectura/parseo: " << ms_read << "\n";
    std::cout << "  Greedy: " << ms_greedy << "\n";
    std::cout << "  Repair light: " << ms_repair << "\n";
    std::cout << "  Local search light: " << ms_ls << "\n";
    std::cout << "  Total: " << ms_total << "\n";
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
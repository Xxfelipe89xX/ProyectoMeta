#include "../include/constructive.hpp"
#include "../include/instance.hpp"
#include "../include/local_search.hpp"
#include "../include/multi_greedy.hpp"
#include "../include/repair.hpp"
#include "../include/solution.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

static int count_visited_customers(const Instance &inst, const Solution &sol) {
  int count = 0;
  std::unordered_map<int, int> seen;
  for (const auto &route : sol.routes) {
    for (int node : route) {
      if (is_customer(inst, node) && seen[node] == 0) {
        seen[node] = 1;
        count++;
      }
    }
  }
  return count;
}

static void print_multi_greedy_summary(const std::vector<Solution> &pool) {
  std::cout << "Cantidad de soluciones: " << pool.size() << "\n\n";

  for (size_t i = 0; i < pool.size(); ++i) {
    const Solution &sol = pool[i];
    std::cout << "Solucion " << (i + 1)
              << " | factible: " << (sol.feasible ? "si" : "no")
              << " | energia: " << sol.total_energy_kwh
              << " | emisiones: " << sol.total_emissions_kg
              << " | rutas: " << sol.routes.size() << "\n";
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Uso:\n";
    std::cerr << "  ./evrp instances/small/C10R2.txt\n";
    std::cerr << "  ./evrp --summary instances/small/C10R2.txt\n";
    std::cerr << "  ./evrp --multi-greedy instances/small/C10R2.txt\n";
    return 1;
  }

  bool summary_mode = false;
  bool multi_greedy_mode = false;
  std::string instance_path;

  if (argc >= 3 && std::string(argv[1]) == "--summary") {
    summary_mode = true;
    instance_path = argv[2];
  } else if (argc >= 3 && std::string(argv[1]) == "--multi-greedy") {
    multi_greedy_mode = true;
    instance_path = argv[2];
  } else {
    instance_path = argv[1];
  }

  try {
    auto t0 = std::chrono::steady_clock::now();

    RawInstance raw = read_raw_instance(instance_path);
    Instance inst = build_internal_instance(raw);

    auto t1 = std::chrono::steady_clock::now();

    if (multi_greedy_mode) {
      MultiGreedyConfig cfg;
      cfg.num_solutions = 10;
      cfg.top_k = 3;
      cfg.apply_repair = true;
      cfg.apply_local_search = true;
      cfg.local_search_iters = 5;
      cfg.seed = 42;
      cfg.sort_output = true;

      std::vector<Solution> pool = generate_greedy_solutions(inst, cfg);

      auto t2 = std::chrono::steady_clock::now();
      auto total_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t0)
              .count();

      print_instance_summary(inst);
      print_multi_greedy_summary(pool);

      std::cout << "\nTiempo total multi-greedy (ms): " << total_ms << "\n";
      return 0;
    }

    Solution sol = greedy_constructive(inst);
    auto t2 = std::chrono::steady_clock::now();

    sol = repair_missing_customers(inst, sol);
    auto t3 = std::chrono::steady_clock::now();

    sol = improve_by_local_search_light(inst, sol, 10);
    auto t4 = std::chrono::steady_clock::now();

    sol = evaluate_solution(inst, sol);

    auto read_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    auto greedy_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    auto repair_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
    auto ls_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count();
    auto total_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t0).count();

    if (summary_mode) {
      std::filesystem::path p(instance_path);
      std::string set_name = p.parent_path().filename().string();
      std::string file_name = p.filename().string();
      int visited = count_visited_customers(inst, sol);

      std::cout << set_name << "," << file_name << ","
                << (sol.feasible ? "1" : "0") << "," << total_ms << ","
                << read_ms << "," << greedy_ms << "," << repair_ms << ","
                << ls_ms << "," << sol.total_energy_kwh << ","
                << sol.total_emissions_kg << "," << sol.routes.size() << ","
                << visited << "," << inst.customer_ids.size() << ","
                << "\"" << sol.error_message << "\""
                << "\n";
      return 0;
    }

    print_instance_summary(inst);
    std::cout << "[Etapa] Greedy constructive...\n";
    std::cout << "[Etapa] Repair...\n";
    std::cout << "[Etapa] Local search light...\n";
    std::cout << "[Etapa] Resultado final\n";
    print_solution(inst, sol);

    std::cout << "\nTiempos (ms):\n";
    std::cout << "  Lectura/parseo: " << read_ms << "\n";
    std::cout << "  Greedy: " << greedy_ms << "\n";
    std::cout << "  Repair: " << repair_ms << "\n";
    std::cout << "  Local search light: " << ls_ms << "\n";
    std::cout << "  Total: " << total_ms << "\n";
  } catch (const std::exception &e) {
    if (summary_mode) {
      std::filesystem::path p(instance_path);
      std::string set_name = p.parent_path().filename().string();
      std::string file_name = p.filename().string();

      std::cout << set_name << "," << file_name << ","
                << "0,0,0,0,0,0,0,0,0,0,0,"
                << "\"" << e.what() << "\"\n";
      return 0;
    }

    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
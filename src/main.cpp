#include "../include/constructive.hpp"
#include "../include/instance.hpp"
#include "../include/local_search.hpp"
#include "../include/multi_greedy.hpp"
#include "../include/repair.hpp"
#include "../include/solution.hpp"
#include "../include/abc.hpp"


#include <chrono>
#include <filesystem>
#include <fstream>
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
  bool abc_mode = false;
  std::string instance_path;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--summary") {
      summary_mode = true;
    } else if (arg == "--multi-greedy") {
      multi_greedy_mode = true;
    } else if (arg == "--abc") {
      abc_mode = true;
    } else {
      instance_path = arg;
    }
  }

  if (instance_path.empty()) {
    std::cerr << "Uso:\n";
    std::cerr << "  ./evrp instances/small/C10R2.txt\n";
    std::cerr << "  ./evrp --summary instances/small/C10R2.txt\n";
    std::cerr << "  ./evrp --multi-greedy instances/small/C10R2.txt\n";
    std::cerr << "  ./evrp --abc instances/small/C10R2.txt\n";
    std::cerr << "  ./evrp --summary --abc instances/small/C10R2.txt\n";
    return 1;
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

      // Serialize pool of solutions to a JSON file
      std::filesystem::create_directories("output/multigreedy_solutions");
      std::filesystem::path p(instance_path);
      std::string instance_name = p.stem().string();
      std::string out_path = "output/multigreedy_solutions/" + instance_name + "_solutions.json";

      std::ofstream out(out_path);
      if (out.is_open()) {
        out << "[\n";
        for (size_t i = 0; i < pool.size(); ++i) {
          const Solution &sol = pool[i];
          out << "  {\n";
          out << "    \"index\": " << (i + 1) << ",\n";
          out << "    \"feasible\": " << (sol.feasible ? "true" : "false") << ",\n";
          out << "    \"energy_kwh\": " << sol.total_energy_kwh << ",\n";
          out << "    \"emissions_kg\": " << sol.total_emissions_kg << ",\n";
          out << "    \"routes\": [\n";
          for (size_t r = 0; r < sol.routes.size(); ++r) {
            out << "      [";
            for (size_t n = 0; n < sol.routes[r].size(); ++n) {
              out << sol.routes[r][n];
              if (n + 1 < sol.routes[r].size()) out << ", ";
            }
            out << "]";
            if (r + 1 < sol.routes.size()) out << ",\n";
            else out << "\n";
          }
          out << "    ]\n";
          if (i + 1 < pool.size()) out << "  },\n";
          else out << "  }\n";
        }
        out << "]\n";
        std::cout << "\n[OK] Soluciones multigreedy guardadas en: " << out_path << "\n";
      } else {
        std::cerr << "\n[Error] No se pudo crear el archivo de soluciones: " << out_path << "\n";
      }

      std::cout << "\nTiempo total multi-greedy (ms): " << total_ms << "\n";
      return 0;
    }

    if (abc_mode) {
      ABCConfig cfg;
      cfg.population_size = 10;
      cfg.limit = 50;
      cfg.max_iterations = 3000;
      cfg.seed = 42;
      cfg.route_elim_prob = 0.5;

      // Run baseline constructive + LS first
      Solution initial_best = greedy_constructive(inst);
      initial_best = repair_missing_customers(inst, initial_best);
      initial_best = improve_by_local_search_light(inst, initial_best, 10);
      initial_best = evaluate_solution(inst, initial_best);

      // Construct JSON solutions path and load it
      std::filesystem::path p(instance_path);
      std::string instance_name = p.stem().string();
      std::string json_path = "output/multigreedy_solutions/" + instance_name + "_solutions.json";
      std::vector<Solution> initial_pop = load_solutions_from_json(json_path);

      auto t_abc_start = std::chrono::steady_clock::now();
      Solution sol = run_artificial_bee_colony(inst, cfg, initial_pop);
      auto t_abc_end = std::chrono::steady_clock::now();
      auto abc_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_abc_end - t_abc_start).count();

      // Export ABC solution to JSON
      std::filesystem::create_directories("output/abc_solutions");
      std::string abc_out_path = "output/abc_solutions/" + instance_name + "_abc.json";
      std::ofstream abc_out(abc_out_path);
      if (abc_out.is_open()) {
        abc_out << "{\n";
        abc_out << "  \"instance\": \"" << instance_name << "\",\n";
        abc_out << "  \"feasible\": " << (sol.feasible ? "true" : "false") << ",\n";
        abc_out << "  \"energy_kwh\": " << sol.total_energy_kwh << ",\n";
        abc_out << "  \"emissions_kg\": " << sol.total_emissions_kg << ",\n";
        abc_out << "  \"num_routes\": " << sol.routes.size() << ",\n";
        abc_out << "  \"abc_time_ms\": " << abc_ms << ",\n";
        abc_out << "  \"initial_energy_kwh\": " << initial_best.total_energy_kwh << ",\n";
        abc_out << "  \"routes\": [\n";
        for (size_t r = 0; r < sol.routes.size(); ++r) {
          abc_out << "    [";
          for (size_t n = 0; n < sol.routes[r].size(); ++n) {
            abc_out << sol.routes[r][n];
            if (n + 1 < sol.routes[r].size()) abc_out << ", ";
          }
          abc_out << "]";
          if (r + 1 < sol.routes.size()) abc_out << ",\n";
          else abc_out << "\n";
        }
        abc_out << "  ]\n";
        abc_out << "}\n";
        abc_out.close();
        std::cerr << "[OK] Solucion ABC guardada en: " << abc_out_path << "\n";
      }

      if (summary_mode) {
        std::filesystem::path p(instance_path);
        std::string set_name = p.parent_path().filename().string();
        std::string file_name = p.filename().string();
        int visited = count_visited_customers(inst, sol);

        std::cout << set_name << "," << file_name << ","
                  << (sol.feasible ? "1" : "0") << "," << abc_ms << ","
                  << (initial_best.feasible ? "1" : "0") << ","
                  << initial_best.total_energy_kwh << ","
                  << sol.total_energy_kwh << ","
                  << sol.total_emissions_kg << "," << sol.routes.size() << ","
                  << visited << "," << inst.customer_ids.size() << ","
                  << "\"" << sol.error_message << "\""
                  << "\n";
        return 0;
      }

      print_instance_summary(inst);
      std::cout << "========== INICIAL (GREEDY + LOCAL SEARCH) ==========\n";
      print_solution(inst, initial_best);

      std::cout << "\n========== RUNNING ARTIFICIAL BEE COLONY (ABC) ==========\n";
      std::cout << "Parámetros:\n";
      std::cout << "  Tamaño población (tau): " << cfg.population_size << "\n";
      std::cout << "  Límite de intentos (limit): " << cfg.limit << "\n";
      std::cout << "  Iteraciones máximas: " << cfg.max_iterations << "\n";
      std::cout << "  Semilla: " << cfg.seed << "\n";
      std::cout << "  Prob. eliminación ruta: " << cfg.route_elim_prob << "\n";

      std::cout << "\n[Etapa] ABC Loop...\n";
      std::cout << "[Etapa] Resultado final ABC:\n";
      print_solution(inst, sol);

      std::cout << "\nTiempo ABC (ms): " << abc_ms << "\n";
      std::cout << "Consumo inicial: " << initial_best.total_energy_kwh << " kWh\n";
      std::cout << "Consumo final ABC: " << sol.total_energy_kwh << " kWh\n";
      std::cout << "Mejora respecto a inicial: " << (initial_best.total_energy_kwh - sol.total_energy_kwh) << " kWh ("
                << (initial_best.total_energy_kwh > 0 ? (initial_best.total_energy_kwh - sol.total_energy_kwh) / initial_best.total_energy_kwh * 100.0 : 0.0)
                << "%)\n";
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
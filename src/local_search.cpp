#include "../include/local_search.hpp"
#include "../include/solution.hpp"

#include <limits>
#include <vector>

struct LocalMove {
  bool feasible = false;
  int route_idx = -1;
  int pos_from = -1;
  int pos_to = -1;
  double gain = 0.0;
};

static LocalMove best_intra_relocate_limited(const Instance &inst,
                                             const Solution &sol) {
  LocalMove best;
  const double EPS = 1e-9;

  for (int r = 0; r < (int)sol.routes.size(); ++r) {
    const auto &route = sol.routes[r];
    RouteEvaluation old_re = evaluate_route(inst, route);
    if (!old_re.feasible)
      continue;

    int customers_checked = 0;

    for (int i = 1; i < (int)route.size() - 1; ++i) {
      int node = route[i];
      if (!is_customer(inst, node))
        continue;

      customers_checked++;
      if (customers_checked > 8)
        break;

      std::vector<int> reduced = route;
      reduced.erase(reduced.begin() + i);

      int positions_checked = 0;
      for (int j = 1; j < (int)reduced.size(); ++j) {
        positions_checked++;
        if (positions_checked > 8)
          break;

        std::vector<int> candidate = reduced;
        candidate.insert(candidate.begin() + j, node);
        if (candidate == route)
          continue;

        RouteEvaluation new_re = evaluate_route(inst, candidate);
        if (!new_re.feasible)
          continue;

        double gain = old_re.total_energy_kwh - new_re.total_energy_kwh;
        if (gain > EPS && (!best.feasible || gain > best.gain)) {
          best.feasible = true;
          best.route_idx = r;
          best.pos_from = i;
          best.pos_to = j;
          best.gain = gain;
        }
      }
    }
  }

  return best;
}

Solution improve_by_local_search_light(const Instance &inst,
                                       const Solution &initial,
                                       int max_improvements) {
  Solution sol = evaluate_solution(inst, initial);
  if (!sol.feasible)
    return sol;

  for (int iter = 0; iter < max_improvements; ++iter) {
    LocalMove best = best_intra_relocate_limited(inst, sol);
    if (!best.feasible)
      break;

    int node = sol.routes[best.route_idx][best.pos_from];
    sol.routes[best.route_idx].erase(sol.routes[best.route_idx].begin() +
                                     best.pos_from);

    int insert_pos = best.pos_to;
    if (best.pos_to > best.pos_from)
      insert_pos--;

    sol.routes[best.route_idx].insert(
        sol.routes[best.route_idx].begin() + insert_pos, node);

    sol = evaluate_solution(inst, sol);
    if (!sol.feasible)
      break;
  }

  return sol;
}
#include "../include/energy.hpp"

double speed_kmh_to_ms(double speed_kmh) { return speed_kmh * 1000.0 / 3600.0; }

double distance_miles_to_meters(const Instance &inst, double miles) {
  return miles * inst.miles_to_meters;
}

double arc_energy_kwh(const Instance &inst, int from, int to,
                      double remaining_load_tons) {
  double d_miles = inst.distance_miles[from][to];
  double d_m = distance_miles_to_meters(inst, d_miles);

  double s_kmh = inst.speed_kmh[from][to];
  double s_ms = speed_kmh_to_ms(s_kmh);

  double load_kg = remaining_load_tons * 1000.0;
  double total_mass = inst.curb_weight_kg + load_kg;

  double energy_joules =
      inst.eff_battery_discharge * inst.eff_motor *
      (inst.alpha * total_mass * d_m + inst.beta * s_ms * s_ms * d_m);

  return energy_joules / 3600000.0;
}

double arc_emissions_kg(const Instance &inst, int from, int to,
                        double remaining_load_tons) {
  double energy = arc_energy_kwh(inst, from, to, remaining_load_tons);
  return energy * inst.eff_recharge * inst.emission_rate_kg_per_kwh;
}
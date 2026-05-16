#ifndef ENERGY_HPP
#define ENERGY_HPP

#include "instance.hpp"

double speed_kmh_to_ms(double speed_kmh);
double distance_miles_to_meters(const Instance &inst, double miles);

double arc_energy_kwh(const Instance &inst, int from, int to,
                      double remaining_load_tons);

double arc_emissions_kg(const Instance &inst, int from, int to,
                        double remaining_load_tons);

#endif
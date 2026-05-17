#ifndef LOCAL_SEARCH_HPP
#define LOCAL_SEARCH_HPP

#include "solution.hpp"

Solution improve_by_local_search_light(const Instance &inst,
                                       const Solution &initial,
                                       int max_improvements = 20);

#endif
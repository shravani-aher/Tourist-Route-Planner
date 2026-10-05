#pragma once

#include "../ds/Graph.h"
#include "../ds/DynArray.h"
#include "Models.h"
#include <string>

namespace planner {

class Tour {
public:
    static RouteResult plan_tour(
        const ds::Graph& graph,
        const RouteQuery& query,
        std::string& error_msg
    );

private:
    static double calculate_leg_travel_time(
        const ds::Graph& graph,
        int u,
        int v,
        const RouteQuery& query
    );

    // Heuristic 1: Nearest-Neighbor greedy ordering of must-visit stops
    static ds::DynArray<int> nearest_neighbor_order(
        const ds::Graph& graph,
        int start_idx,
        int end_idx,
        const ds::DynArray<int>& stops,
        const RouteQuery& query
    );

    // Heuristic 2: Bounded 2-Opt local search improvement
    static void apply_2opt(
        const ds::Graph& graph,
        ds::DynArray<int>& tour,
        const RouteQuery& query,
        int max_iterations = 50
    );
};

} // namespace planner

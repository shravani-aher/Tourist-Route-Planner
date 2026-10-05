#pragma once

#include "../ds/Graph.h"
#include "../ds/DynArray.h"
#include "Models.h"

namespace planner {

class Alternatives {
public:
    static ds::DynArray<RouteResult> generate_ranked_alternatives(
        const ds::Graph& graph,
        int start_idx,
        int end_idx,
        const RouteQuery& query
    );

    // Overlap calculation between two routes (Jaccard coefficient on road sets)
    static double compute_road_overlap(const RouteResult& r1, const RouteResult& r2);
};

} // namespace planner

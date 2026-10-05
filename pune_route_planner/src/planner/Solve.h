#pragma once
#include "Alternatives.h"
#include "Tour.h"
namespace planner {
struct SolveResult {
    RouteResult best;
    ds::DynArray<RouteResult> ranked;
};
// Shared snapshot solver: initial calculation, updates and undo have identical limits.
inline SolveResult solve(const ds::Graph& graph, const RouteQuery& query) {
    SolveResult result;
    if (!query.must_visit.empty()) {
        std::string error;
        result.best = Tour::plan_tour(graph, query, error);
        if (result.best.found) result.ranked.push_back(result.best);
    } else {
        result.ranked = Alternatives::generate_ranked_alternatives(graph,
            graph.get_place_index(query.start_id), graph.get_place_index(query.end_id), query);
        if (!result.ranked.empty()) result.best = result.ranked[0];
        else result.best.message = "No feasible route found in bounded candidate pool";
    }
    return result;
}
} // namespace planner

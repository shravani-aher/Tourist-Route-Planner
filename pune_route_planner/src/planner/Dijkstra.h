#pragma once

#include "../ds/Graph.h"
#include "../ds/MinHeap.h"
#include "../ds/HashMap.h"
#include "Models.h"

namespace planner {

class Dijkstra {
public:
    struct Options {
        Mode mode = Mode::Balanced;
        PreferenceWeights weights;
        ds::DynArray<std::string> interests;
        // Edge penalties for K-shortest / alternative route exploration
        ds::HashMap<std::string, double>* edge_penalties = nullptr;
    };

    static RouteResult find_path(
        const ds::Graph& graph,
        int start_idx,
        int end_idx,
        const Options& options
    );
};

} // namespace planner

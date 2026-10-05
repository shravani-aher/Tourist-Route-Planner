#pragma once

#include "Models.h"
#include "../ds/Graph.h"

namespace planner {

class Scoring {
public:
    static double calculate_mismatch(const ds::Place& target_place, const ds::DynArray<std::string>& interests);

    static double compute_edge_cost(
        const ds::Road& road,
        const ds::Place& target_place,
        Mode mode,
        const PreferenceWeights& weights,
        const ds::DynArray<std::string>& interests,
        double edge_penalty = 0.0
    );

    static double compute_balanced_cost(
        const ds::Road& road,
        const ds::Place& target_place,
        const PreferenceWeights& weights,
        const ds::DynArray<std::string>& interests
    );

    static double compute_demo_index(double balanced_cost);
};

} // namespace planner

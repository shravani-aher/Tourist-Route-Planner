#include "Scoring.h"
#include <algorithm>
#include <cmath>

namespace planner {

double Scoring::calculate_mismatch(const ds::Place& target_place, const ds::DynArray<std::string>& interests) {
    if (interests.empty()) {
        return 0.0;
    }
    for (const auto& interest : interests) {
        if (target_place.has_category(interest)) {
            return 0.0; // match found
        }
    }
    return 1.0; // no matching category
}

double Scoring::compute_balanced_cost(
    const ds::Road& road,
    const ds::Place& target_place,
    const PreferenceWeights& weights,
    const ds::DynArray<std::string>& interests
) {
    double d = road.distance_km;
    double t = road.effective_time_min();
    double scenic_quality = road.scenic / 10.0;
    double crowd = road.crowd / 10.0;
    double mismatch = calculate_mismatch(target_place, interests);

    // balanced = wd*(d/10) + wt*(t/30) + ws*(d/10)*(1-scenic_quality) + wc*(t/30)*crowd + wp*(d/10)*mismatch
    double cost = weights.wd * (d / 10.0)
                + weights.wt * (t / 30.0)
                + weights.ws * (d / 10.0) * (1.0 - scenic_quality)
                + weights.wc * (t / 30.0) * crowd
                + weights.wp * (d / 10.0) * mismatch;

    return std::max(0.0, cost);
}

double Scoring::compute_edge_cost(
    const ds::Road& road,
    const ds::Place& target_place,
    Mode mode,
    const PreferenceWeights& weights,
    const ds::DynArray<std::string>& interests,
    double edge_penalty
) {
    double d = road.distance_km;
    double t = road.effective_time_min();
    double scenic_quality = road.scenic / 10.0;
    double crowd = road.crowd / 10.0;

    double base_cost = 0.0;
    switch (mode) {
        case Mode::Shortest:
            base_cost = d;
            break;
        case Mode::Fastest:
            base_cost = t;
            break;
        case Mode::Scenic:
            // scenic = d * (0.10 + 1 - scenic_quality)
            base_cost = d * (0.10 + 1.0 - scenic_quality);
            break;
        case Mode::LeastCrowded:
            // least_crowded = t * (0.10 + crowd)
            base_cost = t * (0.10 + crowd);
            break;
        case Mode::Balanced:
            base_cost = compute_balanced_cost(road, target_place, weights, interests);
            break;
    }

    if (base_cost < 0.0 || std::isnan(base_cost) || std::isinf(base_cost)) {
        base_cost = 0.0;
    }

    if (edge_penalty > 0.0) {
        base_cost *= (1.0 + edge_penalty);
    }

    return base_cost;
}

double Scoring::compute_demo_index(double balanced_cost) {
    if (balanced_cost < 0.0) return 100.0;
    return 100.0 / (1.0 + balanced_cost);
}

} // namespace planner

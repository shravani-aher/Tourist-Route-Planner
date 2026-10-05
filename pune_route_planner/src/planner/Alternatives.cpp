#include "Alternatives.h"
#include "Dijkstra.h"
#include "Scoring.h"
#include "../ds/Sort.h"
#include "../ds/HashMap.h"
#include <string>
#include <cmath>
#include <unordered_set>

namespace planner {

static std::string get_road_sequence_key(const RouteResult& route) {
    std::string key;
    for (size_t i = 0; i < route.road_path.size(); ++i) {
        key += route.road_path[i];
        key += ";";
    }
    return key;
}

double Alternatives::compute_road_overlap(const RouteResult& r1, const RouteResult& r2) {
    if (r1.road_path.empty() || r2.road_path.empty()) return 0.0;

    std::unordered_set<std::string> set1;
    for (size_t i = 0; i < r1.road_path.size(); ++i) {
        set1.insert(r1.road_path[i]);
    }

    size_t intersection_count = 0;
    std::unordered_set<std::string> union_set = set1;
    for (size_t i = 0; i < r2.road_path.size(); ++i) {
        if (set1.count(r2.road_path[i])) {
            intersection_count++;
        }
        union_set.insert(r2.road_path[i]);
    }

    if (union_set.empty()) return 0.0;
    return static_cast<double>(intersection_count) / static_cast<double>(union_set.size());
}

ds::DynArray<RouteResult> Alternatives::generate_ranked_alternatives(
    const ds::Graph& graph,
    int start_idx,
    int end_idx,
    const RouteQuery& query
) {
    ds::DynArray<RouteResult> candidates;
    ds::HashMap<std::string, bool> seen_sequences;

    // 1. Generate base routes for all 5 modes
    const Mode all_modes[] = {
        Mode::Balanced,
        Mode::Shortest,
        Mode::Fastest,
        Mode::Scenic,
        Mode::LeastCrowded
    };

    Dijkstra::Options base_opts;
    base_opts.weights = query.weights;
    base_opts.interests = query.interests;

    double shortest_dist = -1.0;

    for (Mode m : all_modes) {
        base_opts.mode = m;
        RouteResult res = Dijkstra::find_path(graph, start_idx, end_idx, base_opts);
        if (res.found) {
            std::string seq_key = get_road_sequence_key(res);
            if (!seen_sequences.contains(seq_key)) {
                seen_sequences.insert(seq_key, true);
                candidates.push_back(res);
                if (m == Mode::Shortest || shortest_dist < 0.0 || res.total_distance_km < shortest_dist) {
                    shortest_dist = res.total_distance_km;
                }
            }
        }
    }

    if (candidates.empty()) {
        return candidates;
    }

    // 2. Bounded edge-penalty reruns (up to 20 reruns)
    ds::HashMap<std::string, double> penalties;
    const int kMaxReruns = 20;

    for (int rerun = 0; rerun < kMaxReruns; ++rerun) {
        if (candidates.empty()) break;

        // Apply penalty to roads used in candidate pool
        const RouteResult& last_route = candidates.back();
        for (size_t i = 0; i < last_route.road_path.size(); ++i) {
            const std::string& rid = last_route.road_path[i];
            double* current_pen = penalties.find(rid);
            if (current_pen) {
                *current_pen += 0.50; // compound penalty
            } else {
                penalties.insert(rid, 0.50);
            }
        }

        Dijkstra::Options pen_opts = base_opts;
        pen_opts.mode = query.primary_mode;
        pen_opts.edge_penalties = &penalties;

        RouteResult pen_res = Dijkstra::find_path(graph, start_idx, end_idx, pen_opts);
        if (pen_res.found) {
            std::string seq_key = get_road_sequence_key(pen_res);
            if (!seen_sequences.contains(seq_key)) {
                seen_sequences.insert(seq_key, true);
                pen_res.mode_label = "alternative_" + std::to_string(candidates.size() + 1);
                candidates.push_back(pen_res);
            }
        }
    }

    // 3. Filter candidates by hard limits and max detour ratio (1.75x shortest)
    const double kDetourRatio = 1.75;
    double max_allowed_distance = (shortest_dist > 0.0) ? (shortest_dist * kDetourRatio) : 1e9;

    ds::DynArray<RouteResult> feasible_candidates;
    for (size_t i = 0; i < candidates.size(); ++i) {
        const auto& c = candidates[i];
        if (c.total_distance_km > max_allowed_distance + 1e-6) {
            continue; // violates detour ratio
        }
        if (query.max_distance_km > 0.0 && c.total_distance_km > query.max_distance_km) {
            continue; // violates hard distance limit
        }
        if (query.max_time_min > 0.0 && c.total_travel_time_min > query.max_time_min) {
            continue; // violates hard time limit
        }
        feasible_candidates.push_back(c);
    }

    if (feasible_candidates.empty()) {
        return feasible_candidates;
    }

    // 4. Rank candidates using hand-written merge sort
    auto route_comparator = [](const RouteResult& a, const RouteResult& b) {
        if (std::abs(a.balanced_cost - b.balanced_cost) > 1e-6) {
            return a.balanced_cost < b.balanced_cost;
        }
        if (std::abs(a.total_travel_time_min - b.total_travel_time_min) > 1e-6) {
            return a.total_travel_time_min < b.total_travel_time_min;
        }
        if (std::abs(a.total_distance_km - b.total_distance_km) > 1e-6) {
            return a.total_distance_km < b.total_distance_km;
        }
        // Lexicographical road sequence tie-break
        std::string a_seq = get_road_sequence_key(a);
        std::string b_seq = get_road_sequence_key(b);
        return a_seq < b_seq;
    };

    ds::merge_sort(feasible_candidates, route_comparator);

    // 5. Greedy diversity selection: select up to K routes with road overlap <= 0.70
    ds::DynArray<RouteResult> selected;
    int k_limit = query.k_alternatives > 0 ? query.k_alternatives : 5;
    const double kMaxOverlap = 0.70;

    for (size_t i = 0; i < feasible_candidates.size() && static_cast<int>(selected.size()) < k_limit; ++i) {
        const auto& cand = feasible_candidates[i];
        bool too_similar = false;
        for (size_t j = 0; j < selected.size(); ++j) {
            if (compute_road_overlap(cand, selected[j]) >= kMaxOverlap) {
                too_similar = true;
                break;
            }
        }
        if (!too_similar) {
            selected.push_back(cand);
        }
    }

    return selected;
}

} // namespace planner

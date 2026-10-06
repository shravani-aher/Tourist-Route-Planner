#include "Tour.h"
#include "Dijkstra.h"
#include "Scoring.h"
#include "TimeContext.h"
#include <map>
#include "../ds/Sort.h"
#include <algorithm>
#include <cmath>

namespace planner {

struct LegInfo { bool found = false; double time = 0, dist = 0; };
static std::map<std::pair<int,int>, LegInfo>& leg_cache() { static thread_local std::map<std::pair<int,int>, LegInfo> c; return c; }
static const LegInfo& get_leg(const ds::Graph& graph, int u, int v, const RouteQuery& query) {
    auto key = std::make_pair(u, v); auto& c = leg_cache(); auto it = c.find(key);
    if (it != c.end()) return it->second;
    LegInfo info;
    if (u == v) { info.found = true; }
    else {
        Dijkstra::Options opts; opts.mode = query.primary_mode; opts.weights = query.weights; opts.interests = query.interests;
        auto r = Dijkstra::find_path(graph, u, v, opts);
        info.found = r.found; info.time = r.total_travel_time_min; info.dist = r.total_distance_km;
    }
    return c.emplace(key, info).first->second;
}
// Walks the itinerary from the departure time. Fills stop timings; false (with reason) if a stop is closed on arrival.
static bool schedule_tour(const ds::Graph& graph, const ds::DynArray<int>& tour, const RouteQuery& query,
                          ds::DynArray<PlannedStop>* out, std::string* why) {
    double now = time_context().hour * 60.0;
    for (size_t i = 0; i < tour.size(); ++i) {
        if (i > 0) { const auto& l = get_leg(graph, tour[i-1], tour[i], query); if (!l.found) { if (why) *why = "unreachable leg"; return false; } now += l.time; }
        const auto& p = graph.get_place(tour[i]);
        bool visited = i > 0 && i + 1 < tour.size();
        PlannedStop st; st.place_id = p.id; st.name = p.name; st.is_start = i == 0; st.is_end = i + 1 == tour.size();
        st.arrive_min = (int)std::lround(now);
        if (visited) {
            int arrive = (int)std::lround(now);
            if (p.metrics_estimated) {
                int open = p.open_hour * 60, close = p.close_hour * 60;
                if (arrive < open) { st.wait_min = open - arrive; now = open; }
                if (now + p.visit_minutes > close + 1e-6) { if (why) *why = p.name + " would close (" + std::to_string(p.close_hour) + ":00, estimated hours) before the visit ends if you arrive at " + std::to_string(arrive / 60) + ":" + (arrive % 60 < 10 ? "0" : "") + std::to_string(arrive % 60); return false; }
                int h = std::min(23, (int)(now / 60)); st.crowd_at_arrival = time_context().weekend ? p.crowd_weekend[h] : p.crowd_weekday[h];
            }
            st.visit_minutes = p.visit_minutes; now += p.visit_minutes;
        }
        st.depart_min = (int)std::lround(now);
        if (out) out->push_back(st);
    }
    return true;
}
// Evaluate every directed leg, including reversed interior arcs after 2-opt.
static bool measure_tour(const ds::Graph& graph, const ds::DynArray<int>& tour,
                         const RouteQuery& query, double& time, double& distance) {
    time = distance = 0.0;
    for (size_t i = 0; i + 1 < tour.size(); ++i) {
        const auto& leg = get_leg(graph, tour[i], tour[i + 1], query);
        if (!leg.found) return false;
        time += leg.time; distance += leg.dist;
    }
    for (size_t i = 1; i + 1 < tour.size(); ++i) time += graph.get_place(tour[i]).visit_minutes;
    return true;
}
static bool within_limits(double time, double distance, const RouteQuery& query) {
    return (query.max_time_min <= 0 || time <= query.max_time_min + 1e-6) &&
           (query.max_distance_km <= 0 || distance <= query.max_distance_km + 1e-6);
}

double Tour::calculate_leg_travel_time(
    const ds::Graph& graph,
    int u,
    int v,
    const RouteQuery& query
) {
    if (u == v) return 0.0;
    const auto& l = get_leg(graph, u, v, query);
    return l.found ? l.time : 1e9;
}

ds::DynArray<int> Tour::nearest_neighbor_order(
    const ds::Graph& graph,
    int start_idx,
    int end_idx,
    const ds::DynArray<int>& stops,
    const RouteQuery& query
) {
    ds::DynArray<int> ordered;
    ordered.push_back(start_idx);

    ds::DynArray<int> unvisited = stops;
    int current = start_idx;

    while (!unvisited.empty()) {
        int best_idx = -1;
        double best_time = 1e9;
        size_t best_pos = 0;

        for (size_t i = 0; i < unvisited.size(); ++i) {
            int candidate = unvisited[i];
            double t = calculate_leg_travel_time(graph, current, candidate, query);
            if (t < best_time) {
                best_time = t;
                best_idx = candidate;
                best_pos = i;
            }
        }

        if (best_idx == -1) {
            // Unreachable fallback: append remaining in default order
            for (size_t i = 0; i < unvisited.size(); ++i) {
                ordered.push_back(unvisited[i]);
            }
            break;
        }

        ordered.push_back(best_idx);
        current = best_idx;

        // Remove best_idx from unvisited
        for (size_t i = best_pos; i + 1 < unvisited.size(); ++i) {
            unvisited[i] = unvisited[i + 1];
        }
        unvisited.pop_back();
    }

    ordered.push_back(end_idx);
    return ordered;
}

void Tour::apply_2opt(
    const ds::Graph& graph,
    ds::DynArray<int>& tour,
    const RouteQuery& query,
    int max_iterations
) {
    // 2-Opt local search: reverses sub-segments between intermediate stops
    // (indices 1 to tour.size() - 2) to eliminate route self-crossings.
    // Note: This is an O(K^2) heuristic improvement, not an exact TSP solver.
    if (tour.size() <= 3) return;

    bool improved = true;
    int iter = 0;

    while (improved && iter < max_iterations) {
        improved = false;
        ++iter;

        for (size_t i = 1; i + 1 < tour.size() - 1; ++i) {
            for (size_t j = i + 1; j < tour.size() - 1; ++j) {
                double old_time, old_distance, new_time, new_distance;
                auto candidate = tour;
                std::reverse(candidate.begin() + i, candidate.begin() + j + 1);
                if (measure_tour(graph, tour, query, old_time, old_distance) &&
                    measure_tour(graph, candidate, query, new_time, new_distance) &&
                    new_time < old_time - 1e-4 &&
                    (query.max_distance_km <= 0 || new_distance <= query.max_distance_km + 1e-6)) {
                    tour = std::move(candidate);
                    improved = true;
                }
            }
        }
    }
}

RouteResult Tour::plan_tour(
    const ds::Graph& graph,
    const RouteQuery& query,
    std::string& error_msg
) {
    RouteResult final_route;
    error_msg.clear();
    leg_cache().clear();
    final_route.mode_label = "personalized_tour";

    int start_idx = graph.get_place_index(query.start_id);
    int end_idx = graph.get_place_index(query.end_id);

    if (start_idx < 0) {
        error_msg = "Start place ID not found: " + query.start_id;
        final_route.message = error_msg;
        return final_route;
    }
    if (end_idx < 0) {
        error_msg = "End place ID not found: " + query.end_id;
        final_route.message = error_msg;
        return final_route;
    }

    // 1. Conflict detection: Check if any must-visit stop matches an avoided category
    for (size_t i = 0; i < query.must_visit.size(); ++i) {
        const std::string& mv_id = query.must_visit[i];
        const ds::Place* p = graph.get_place(mv_id);
        if (!p) {
            error_msg = "Must-visit place ID not found: " + mv_id;
            final_route.message = error_msg;
            return final_route;
        }
        for (size_t a = 0; a < query.avoid.size(); ++a) {
            if (p->has_category(query.avoid[a])) {
                error_msg = "Conflict detected: Must-visit place '" + p->name +
                            "' (" + p->id + ") has avoided category '" + query.avoid[a] + "'.";
                final_route.message = error_msg;
                return final_route;
            }
        }
    }

    // 2. Identify must-visit node indices
    ds::DynArray<int> must_visit_indices;
    for (size_t i = 0; i < query.must_visit.size(); ++i) {
        int idx = graph.get_place_index(query.must_visit[i]);
        if (idx != start_idx && idx != end_idx) {
            // Avoid duplicate entries
            bool already = false;
            for (size_t j = 0; j < must_visit_indices.size(); ++j) {
                if (must_visit_indices[j] == idx) { already = true; break; }
            }
            if (!already) {
                must_visit_indices.push_back(idx);
            }
        }
    }

    // 3. Initial tour sequence via Nearest-Neighbor heuristic, then 2-opt refinement
    ds::DynArray<int> current_tour = nearest_neighbor_order(
        graph, start_idx, end_idx, must_visit_indices, query
    );
    apply_2opt(graph, current_tour, query);

    double current_total_time, current_distance;
    if (!measure_tour(graph, current_tour, query, current_total_time, current_distance)) {
        error_msg = "Cannot reach mandatory tour stops"; final_route.message = error_msg; return final_route;
    }
    if (!within_limits(current_total_time, current_distance, query)) {
        error_msg = "Mandatory tour exceeds time or distance limit for the selected mode";
        final_route.message = error_msg; return final_route;
    }
    if (graph.real_data) { std::string why; if (!schedule_tour(graph, current_tour, query, nullptr, &why)) { error_msg = "Mandatory stop not feasible at this departure time: " + why; final_route.message = error_msg; return final_route; } }
    // Optional visits are inserted only if the entire selected-mode itinerary fits.
    if (query.max_time_min > 0.0) {
        struct OptionalCandidate {
            int node_idx = -1;
            double benefit = 0.0;
            int visit_min = 0;
            double ratio = 0.0;
        };

        ds::DynArray<OptionalCandidate> optional_pool;
        for (size_t i = 0; i < graph.num_places(); ++i) {
            int idx = static_cast<int>(i);
            if (idx == start_idx || idx == end_idx) continue;

            // Check if already in tour
            bool in_tour = false;
            for (size_t j = 0; j < current_tour.size(); ++j) {
                if (current_tour[j] == idx) { in_tour = true; break; }
            }
            if (in_tour) continue;

            const ds::Place& p = graph.get_place(idx);
            if (graph.real_data && (!p.attraction || p.visit_minutes <= 0)) continue;

            // Exclude avoided categories (Avoided categories exclude optional visits, not transit)
            bool is_avoided = false;
            for (size_t a = 0; a < query.avoid.size(); ++a) {
                if (p.has_category(query.avoid[a])) {
                    is_avoided = true;
                    break;
                }
            }
            if (is_avoided) continue;

            // Calculate benefit
            double benefit = 0.0;
            if (query.interests.empty()) {
                benefit = 1.0 + (10.0 - p.crowd) * 0.1; // fallback preference for scenic/less crowd
            } else {
                for (size_t c = 0; c < query.interests.size(); ++c) {
                    if (p.has_category(query.interests[c])) {
                        benefit += 1.0;
                    }
                }
            }

            if (benefit > 0.0) {
                double r = benefit / static_cast<double>(p.visit_minutes);
                optional_pool.push_back(OptionalCandidate{idx, benefit, p.visit_minutes, r});
            }
        }

        // Sort candidates by benefit/time ratio descending using merge_sort
        ds::merge_sort(optional_pool, [](const OptionalCandidate& a, const OptionalCandidate& b) {
            return a.ratio > b.ratio; // descending
        });

        if (optional_pool.size() > 6) { ds::DynArray<OptionalCandidate> top; for (size_t c = 0; c < 6; ++c) top.push_back(optional_pool[c]); optional_pool = std::move(top); }
        // Greedy insertion of candidates into tour at position minimizing travel time increase
        for (size_t c = 0; c < optional_pool.size(); ++c) {
            int cand_idx = optional_pool[c].node_idx;
            ds::DynArray<int> best_tour;
            double best_time = 1e100, best_distance = 0.0;
            for (size_t pos = 1; pos < current_tour.size(); ++pos) {
                ds::DynArray<int> candidate;
                for (size_t i = 0; i < current_tour.size(); ++i) {
                    if (i == pos) candidate.push_back(cand_idx);
                    candidate.push_back(current_tour[i]);
                }
                double time, distance;
                if (measure_tour(graph, candidate, query, time, distance) &&
                    within_limits(time, distance, query) && time < best_time &&
                    schedule_tour(graph, candidate, query, nullptr, nullptr)) {
                    best_time = time; best_distance = distance; best_tour = std::move(candidate);
                }
            }
            if (!best_tour.empty()) {
                current_tour = std::move(best_tour);
                current_total_time = best_time; current_distance = best_distance;
            }
        }
    }

    // 6. Connect consecutive stops with Dijkstra paths
    Dijkstra::Options leg_opts;
    leg_opts.mode = query.primary_mode;
    leg_opts.weights = query.weights;
    leg_opts.interests = query.interests;

    double total_scenic_weighted = 0.0;
    double total_crowd_weighted = 0.0;

    for (size_t s = 0; s + 1 < current_tour.size(); ++s) {
        int u = current_tour[s];
        int v = current_tour[s + 1];

        // Record planned stop for node u
        const auto& pu = graph.get_place(u);
        bool is_start_node = (s == 0);
        bool is_must = false;
        for (size_t m = 0; m < query.must_visit.size(); ++m) {
            if (query.must_visit[m] == pu.id) { is_must = true; break; }
        }
        int visit_min = is_start_node ? 0 : pu.visit_minutes;
        final_route.stops.push_back(PlannedStop{pu.id, pu.name, visit_min, is_must, is_start_node, false});
        final_route.total_visit_time_min += visit_min;

        RouteResult leg_result = Dijkstra::find_path(graph, u, v, leg_opts);
        if (!leg_result.found) {
            error_msg = "Could not find valid path between " + pu.name + " and " + graph.get_place(v).name;
            final_route.found = false;
            final_route.message = error_msg;
            return final_route;
        }

        // Append leg elements
        if (s == 0) {
            final_route.node_path.push_back(u);
        }
        for (size_t i = 1; i < leg_result.node_path.size(); ++i) {
            final_route.node_path.push_back(leg_result.node_path[i]);
        }
        for (size_t i = 0; i < leg_result.road_path.size(); ++i) {
            final_route.road_path.push_back(leg_result.road_path[i]);
        }
        for (size_t i = 0; i < leg_result.legs.size(); ++i) {
            final_route.legs.push_back(leg_result.legs[i]);
        }

        final_route.total_distance_km += leg_result.total_distance_km;
        final_route.total_travel_time_min += leg_result.total_travel_time_min;
        total_scenic_weighted += leg_result.avg_scenic * leg_result.total_distance_km;
        total_crowd_weighted += leg_result.avg_crowd * leg_result.total_travel_time_min;
        final_route.balanced_cost += leg_result.balanced_cost;

        for (size_t i = 0; i < leg_result.settled_order.size(); ++i) {
            final_route.settled_order.push_back(leg_result.settled_order[i]);
        }
    }

    // Add final stop
    int last_idx = current_tour.back();
    const auto& last_p = graph.get_place(last_idx);
    final_route.stops.push_back(PlannedStop{last_p.id, last_p.name, 0, false, false, true});
    if (graph.real_data) {
        ds::DynArray<PlannedStop> timed; std::string why;
        if (schedule_tour(graph, current_tour, query, &timed, &why)) {
            for (size_t i = 0; i < final_route.stops.size() && i < timed.size(); ++i) {
                final_route.stops[i].arrive_min = timed[i].arrive_min; final_route.stops[i].depart_min = timed[i].depart_min;
                final_route.stops[i].wait_min = timed[i].wait_min; final_route.stops[i].crowd_at_arrival = timed[i].crowd_at_arrival;
                final_route.total_visit_time_min += timed[i].wait_min;
            }
        }
    }

    if (final_route.total_distance_km > 0.0) {
        final_route.avg_scenic = total_scenic_weighted / final_route.total_distance_km;
    }
    if (final_route.total_travel_time_min > 0.0) {
        final_route.avg_crowd = total_crowd_weighted / final_route.total_travel_time_min;
    }

    final_route.total_time_min = final_route.total_travel_time_min + final_route.total_visit_time_min;
    final_route.demo_index = Scoring::compute_demo_index(final_route.balanced_cost);
    if (!within_limits(final_route.total_time_min, final_route.total_distance_km, query)) {
        error_msg = "Final itinerary exceeds time or distance limit";
        final_route.message = error_msg; return final_route;
    }
    final_route.found = true;
    final_route.message = "Personalized tour planned successfully (" +
                          std::to_string(current_tour.size()) + " stops).";

    return final_route;
}

} // namespace planner

#include "Dijkstra.h"
#include "Scoring.h"
#include <limits>
#include <cmath>
#include <algorithm>

namespace planner {

RouteResult Dijkstra::find_path(
    const ds::Graph& graph,
    int start_idx,
    int end_idx,
    const Options& options
) {
    RouteResult result;
    result.mode_label = mode_to_string(options.mode);

    size_t num_nodes = graph.num_places();
    if (start_idx < 0 || static_cast<size_t>(start_idx) >= num_nodes ||
        end_idx < 0 || static_cast<size_t>(end_idx) >= num_nodes) {
        result.found = false;
        result.message = "Invalid start or end node index";
        return result;
    }

    // Special case: start == end (zero-edge route)
    if (start_idx == end_idx) {
        result.found = true;
        result.node_path.push_back(start_idx);
        result.settled_order.push_back(start_idx);
        const auto& p = graph.get_place(start_idx);
        result.stops.push_back(PlannedStop{p.id, p.name, p.visit_minutes, false, true, true});
        result.demo_index = 100.0;
        result.message = "Start and destination are identical; 0 km route.";
        return result;
    }

    const double kInfinity = std::numeric_limits<double>::infinity();
    ds::DynArray<double> dist(num_nodes, kInfinity);
    ds::DynArray<int> pred_node(num_nodes, -1);
    ds::DynArray<std::string> pred_road(num_nodes, "");
    ds::DynArray<bool> settled(num_nodes, false);

    ds::MinHeap<double, int> pq(num_nodes);

    dist[start_idx] = 0.0;
    pq.push(start_idx, 0.0);

    while (!pq.empty()) {
        auto top = pq.pop();
        int u = top.id;
        double u_dist = top.priority;

        if (settled[u]) continue;
        settled[u] = true;
        result.settled_order.push_back(u);

        if (u == end_idx) {
            // Target settled
            break;
        }

        const auto& arcs = graph.get_outgoing_arcs(u);
        for (const auto& arc : arcs) {
            int v = arc.to;
            if (settled[v]) continue;

            const ds::Road* road = graph.get_road(arc.roadId);
            if (!road || road->blocked) {
                continue;
            }

            double penalty = 0.0;
            if (options.edge_penalties) {
                const double* pen = options.edge_penalties->find(road->id);
                if (pen) penalty = *pen;
            }

            const auto& v_place = graph.get_place(v);
            double edge_cost = Scoring::compute_edge_cost(
                *road, v_place, options.mode, options.weights, options.interests, penalty
            );

            if (edge_cost < 0.0 || std::isnan(edge_cost) || std::isinf(edge_cost)) {
                continue; // reject invalid cost
            }

            double cand_dist = u_dist + edge_cost;

            bool improve = false;
            if (cand_dist < dist[v] - 1e-9) {
                improve = true;
            } else if (std::abs(cand_dist - dist[v]) <= 1e-9) {
                // Deterministic tie-break by node ID, then road ID
                std::string u_id = graph.get_place(u).id;
                std::string curr_pred_u_id = (pred_node[v] >= 0) ? graph.get_place(pred_node[v]).id : "";
                if (pred_node[v] < 0 || u_id < curr_pred_u_id) {
                    improve = true;
                } else if (u_id == curr_pred_u_id && arc.roadId < pred_road[v]) {
                    improve = true;
                }
            }

            if (improve) {
                dist[v] = cand_dist;
                pred_node[v] = u;
                pred_road[v] = arc.roadId;
                pq.push(v, cand_dist);
            }
        }
    }

    if (!settled[end_idx] || dist[end_idx] == kInfinity) {
        result.found = false;
        result.message = "Destination unreachable from start node due to road closures or disconnected graph components.";
        return result;
    }

    // Reconstruct path backwards from end_idx to start_idx
    ds::DynArray<int> rev_nodes;
    ds::DynArray<std::string> rev_roads;
    int curr = end_idx;

    while (curr != start_idx && curr >= 0) {
        rev_nodes.push_back(curr);
        rev_roads.push_back(pred_road[curr]);
        curr = pred_node[curr];
    }
    rev_nodes.push_back(start_idx);

    // Reverse to get start -> end order
    for (size_t i = 0; i < rev_nodes.size(); ++i) {
        result.node_path.push_back(rev_nodes[rev_nodes.size() - 1 - i]);
    }
    for (size_t i = 0; i < rev_roads.size(); ++i) {
        result.road_path.push_back(rev_roads[rev_roads.size() - 1 - i]);
    }

    // Compute ground-truth metrics (re-evaluate on unpenalized original costs)
    double total_scenic_weighted = 0.0;
    double total_crowd_weighted = 0.0;

    for (size_t i = 0; i < result.road_path.size(); ++i) {
        const std::string& r_id = result.road_path[i];
        int u_node = result.node_path[i];
        int v_node = result.node_path[i + 1];

        const ds::Road* r = graph.get_road(r_id);
        if (!r) continue;

        RouteLeg leg;
        leg.road_id = r->id;
        leg.from_idx = u_node;
        leg.to_idx = v_node;
        leg.from_id = graph.get_place(u_node).id;
        leg.to_id = graph.get_place(v_node).id;
        leg.distance_km = r->distance_km;
        leg.travel_time_min = r->effective_time_min();
        leg.scenic = r->scenic;
        leg.crowd = r->crowd;
        leg.traffic = r->traffic;
        result.legs.push_back(leg);

        result.total_distance_km += r->distance_km;
        result.total_travel_time_min += r->effective_time_min();
        total_scenic_weighted += r->scenic * r->distance_km;
        total_crowd_weighted += r->crowd * r->effective_time_min();

        const auto& v_place = graph.get_place(v_node);
        result.balanced_cost += Scoring::compute_balanced_cost(
            *r, v_place, options.weights, options.interests
        );
    }

    if (result.total_distance_km > 0.0) {
        result.avg_scenic = total_scenic_weighted / result.total_distance_km;
    }
    if (result.total_travel_time_min > 0.0) {
        result.avg_crowd = total_crowd_weighted / result.total_travel_time_min;
    }

    result.total_time_min = result.total_travel_time_min;
    result.demo_index = Scoring::compute_demo_index(result.balanced_cost);
    result.found = true;
    result.message = "Route computed successfully";

    return result;
}

} // namespace planner

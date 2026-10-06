#include "Dijkstra.h"
#include "Scoring.h"
#include "TimeContext.h"
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
        result.stops.push_back(PlannedStop{p.id, p.name, 0, false, true, true});
        result.demo_index = 100.0;
        result.message = "Start and destination are identical; 0 km route.";
        return result;
    }

    // State = directed arrival arc, not vertex. This preserves incoming-way
    // context for turn restrictions. Use the hand-built hash map and min-heap.
    struct State { int node; std::string incoming; int predecessor; double distance; bool settled; };
    ds::DynArray<State> states;
    ds::HashMap<std::string, int> ids;
    ds::MinHeap<double, int> pq;
    auto key = [](int node, const std::string& road) { return std::to_string(node) + ":" + road; };
    states.push_back(State{start_idx, "", -1, 0.0, false});
    ids.insert(key(start_idx, ""), 0); pq.push(0, 0.0);
    int target = -1;
    while (!pq.empty()) {
        int sid = pq.pop().id;
        if (states[sid].settled) continue;
        states[sid].settled = true;
        // Copy before growing states, which can invalidate references.
        State current = states[sid]; int u = current.node;
        result.settled_order.push_back(u);
        if (u == end_idx) { target = sid; break; }
        const ds::Road* previous = current.incoming.empty() ? nullptr : graph.get_road(current.incoming);
        for (const auto& arc : graph.get_outgoing_arcs(u)) {
            const auto* road = graph.get_road(arc.roadId);
            if (!road || road->blocked) continue;
            if (previous && !graph.turn_allowed(u, previous->osm_way, road->osm_way)) continue;
            double penalty = 0.0;
            if (options.edge_penalties) { const auto* p = options.edge_penalties->find(road->id); if (p) penalty = *p; }
            double cost = Scoring::compute_edge_cost(*road, graph.get_place(arc.to), options.mode,
                options.weights, options.interests, penalty);
            if (!std::isfinite(cost) || cost < 0) continue;
            std::string k = key(arc.to, arc.roadId);
            const int* existing = ids.find(k); int next;
            if (existing) next = *existing;
            else { next = static_cast<int>(states.size()); ids.insert(k,next);
                states.push_back(State{arc.to, arc.roadId, -1, std::numeric_limits<double>::infinity(), false}); }
            double candidate = current.distance + cost;
            if (!states[next].settled && candidate < states[next].distance - 1e-12) {
                states[next].distance = candidate; states[next].predecessor = sid; pq.push(next,candidate);
            }
        }
    }
    if (target < 0) { result.message = "Destination unreachable within eligible bounded street network."; return result; }
    ds::DynArray<int> rev_nodes; ds::DynArray<std::string> rev_roads;
    for (int cur=target; cur>=0; cur=states[cur].predecessor) {
        rev_nodes.push_back(states[cur].node);
        if (!states[cur].incoming.empty()) rev_roads.push_back(states[cur].incoming);
    }
    for (size_t i=0; i<rev_nodes.size(); ++i) result.node_path.push_back(rev_nodes[rev_nodes.size()-1-i]);
    for (size_t i=0; i<rev_roads.size(); ++i) result.road_path.push_back(rev_roads[rev_roads.size()-1-i]);

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
        leg.crowd = road_crowd(*r);
        leg.traffic = r->traffic;
        result.legs.push_back(leg);

        result.total_distance_km += r->distance_km;
        result.total_travel_time_min += r->effective_time_min();
        total_scenic_weighted += r->scenic * r->distance_km;
        total_crowd_weighted += road_crowd(*r) * r->effective_time_min();

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

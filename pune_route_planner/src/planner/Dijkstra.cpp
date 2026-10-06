#include "Dijkstra.h"
#include "Scoring.h"
#include "TimeContext.h"
#include <limits>
#include <cmath>
#include <algorithm>
#include <vector>

namespace {
struct Flat {
    const void* identity = nullptr; size_t np = 0, nr = 0;
    std::vector<int> first, to; std::vector<const ds::Road*> road; std::vector<char> has_turn;
};
// Rebuilt only when the graph object or its size changes. Road pointers stay valid because
// roads are never inserted after load; blocked/traffic flags are read live through them.
const Flat& flatten(const ds::Graph& g) {
    static Flat f;
    const void* id = g.num_places() ? static_cast<const void*>(&g.get_outgoing_arcs(0)) : nullptr;
    if (f.identity == id && f.np == g.num_places() && f.nr == g.num_roads() && id) return f;
    f = Flat(); f.identity = id; f.np = g.num_places(); f.nr = g.num_roads();
    f.first.assign(f.np + 1, 0); f.has_turn.assign(f.np, 0);
    for (size_t u = 0; u < f.np; ++u) {
        f.first[u] = static_cast<int>(f.to.size());
        f.has_turn[u] = g.turns.find(g.get_place(static_cast<int>(u)).id) ? 1 : 0;
        for (const auto& arc : g.get_outgoing_arcs(static_cast<int>(u))) { f.to.push_back(arc.to); f.road.push_back(g.get_road(arc.roadId)); }
    }
    f.first[f.np] = static_cast<int>(f.to.size());
    return f;
}
}

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

    // State = directed arrival arc, not vertex, so incoming-way context drives turn
    // restrictions. Arcs are flattened once per graph into integer arrays (no string
    // hashing in the hot loop). State 0 is the start; state a+1 is arc a.
    const Flat& flat = flatten(graph);
    const size_t num_arcs = flat.to.size();
    std::vector<double> dist(num_arcs + 1, std::numeric_limits<double>::infinity());
    std::vector<int> pred(num_arcs + 1, -1);
    std::vector<char> settled(num_arcs + 1, 0);
    ds::MinHeap<double, int> pq;
    dist[0] = 0.0; pq.push(0, 0.0);
    int target = -1;
    while (!pq.empty()) {
        int sid = pq.pop().id;
        if (settled[sid]) continue;
        settled[sid] = 1;
        int u = sid == 0 ? start_idx : flat.to[sid - 1];
        result.settled_order.push_back(u);
        if (u == end_idx) { target = sid; break; }
        const ds::Road* previous = sid == 0 ? nullptr : flat.road[sid - 1];
        const bool check_turns = previous && flat.has_turn[u];
        for (int a = flat.first[u]; a < flat.first[u + 1]; ++a) {
            const ds::Road* road = flat.road[a];
            if (!road || road->blocked) continue;
            if (check_turns && !graph.turn_allowed(u, previous->osm_way, road->osm_way)) continue;
            double penalty = 0.0;
            if (options.edge_penalties) { const auto* p = options.edge_penalties->find(road->id); if (p) penalty = *p; }
            double cost = Scoring::compute_edge_cost(*road, graph.get_place(flat.to[a]), options.mode,
                options.weights, options.interests, penalty);
            if (!std::isfinite(cost) || cost < 0) continue;
            int next = a + 1;
            double candidate = dist[sid] + cost;
            if (!settled[next] && candidate < dist[next] - 1e-12) {
                dist[next] = candidate; pred[next] = sid; pq.push(next, candidate);
            }
        }
    }
    if (target < 0) { result.message = "Destination unreachable within eligible bounded street network."; return result; }
    ds::DynArray<int> rev_nodes; ds::DynArray<std::string> rev_roads;
    for (int cur = target; cur >= 0; cur = pred[cur]) {
        rev_nodes.push_back(cur == 0 ? start_idx : flat.to[cur - 1]);
        if (cur != 0) rev_roads.push_back(flat.road[cur - 1]->id);
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

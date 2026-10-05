#include "Dynamic.h"
#include "Dijkstra.h"
#include "Alternatives.h"
#include "Tour.h"
#include "Solve.h"
#include <unordered_set>
#include <sstream>
#include <iomanip>

namespace planner {

DynamicManager::DynamicManager(ds::Graph& g) : graph_(g) {
    init_default_events();
}

void DynamicManager::set_active_query(const RouteQuery& q, const RouteResult& initial_route) {
    has_active_query_ = true;
    last_query_ = q;
    last_route_ = initial_route;
}

bool DynamicManager::check_reachability_bfs(
    int start_idx,
    const ds::DynArray<int>& targets,
    ds::DynArray<int>* unreachable_targets
) const {
    if (start_idx < 0 || static_cast<size_t>(start_idx) >= graph_.num_places()) {
        return false;
    }

    ds::DynArray<bool> visited(graph_.num_places(), false);
    ds::Queue<int> q;

    visited[start_idx] = true;
    q.push(start_idx);

    while (!q.empty()) {
        int u = q.pop();
        const auto& arcs = graph_.get_outgoing_arcs(u);
        for (const auto& arc : arcs) {
            const ds::Road* r = graph_.get_road(arc.roadId);
            if (!r || r->blocked) continue;

            int v = arc.to;
            if (!visited[v]) {
                visited[v] = true;
                q.push(v);
            }
        }
    }

    bool all_reachable = true;
    for (size_t i = 0; i < targets.size(); ++i) {
        int t = targets[i];
        if (t >= 0 && static_cast<size_t>(t) < graph_.num_places()) {
            if (!visited[t]) {
                all_reachable = false;
                if (unreachable_targets) {
                    unreachable_targets->push_back(t);
                }
            }
        }
    }

    return all_reachable;
}

int DynamicManager::compute_connected_components(ds::DynArray<int>* node_component_id) const {
    size_t n = graph_.num_places();
    ds::UnionFind uf(n);

    const auto& road_ids = graph_.all_road_ids();
    for (size_t i = 0; i < road_ids.size(); ++i) {
        const ds::Road* r = graph_.get_road(road_ids[i]);
        if (r && !r->blocked) {
            uf.unite(r->u_idx, r->v_idx);
        }
    }

    if (node_component_id) {
        node_component_id->clear();
        node_component_id->reserve(n);
        for (size_t i = 0; i < n; ++i) {
            node_component_id->push_back(uf.find(static_cast<int>(i)));
        }
    }

    return uf.component_count();
}

RouteDiff DynamicManager::compute_diff(const RouteResult& old_r, const RouteResult& new_r) {
    RouteDiff diff;
    diff.has_old = old_r.found;
    diff.has_new = new_r.found;

    if (!old_r.found && !new_r.found) {
        diff.summary = "Both prior and updated routes are infeasible.";
        return diff;
    }
    if (!old_r.found && new_r.found) {
        diff.summary = "Route restored! Previously unreachable path is now open.";
        return diff;
    }
    if (old_r.found && !new_r.found) {
        diff.summary = "Route disrupted! Road closure has made the destination or must-visit stop unreachable.";
        return diff;
    }

    diff.delta_distance_km = new_r.total_distance_km - old_r.total_distance_km;
    diff.delta_time_min = new_r.total_travel_time_min - old_r.total_travel_time_min;
    diff.delta_balanced_cost = new_r.balanced_cost - old_r.balanced_cost;

    std::unordered_set<std::string> old_roads;
    for (size_t i = 0; i < old_r.road_path.size(); ++i) {
        old_roads.insert(old_r.road_path[i]);
    }

    std::unordered_set<std::string> new_roads;
    for (size_t i = 0; i < new_r.road_path.size(); ++i) {
        new_roads.insert(new_r.road_path[i]);
    }

    for (const auto& r : new_roads) {
        if (!old_roads.count(r)) {
            diff.roads_added.push_back(r);
        }
    }
    for (const auto& r : old_roads) {
        if (!new_roads.count(r)) {
            diff.roads_removed.push_back(r);
        }
    }

    std::ostringstream ss;
    if (diff.roads_added.empty() && diff.roads_removed.empty()) {
        ss << "Route corridor unchanged. Time delta: "
           << std::showpos << std::fixed << std::setprecision(1) << diff.delta_time_min << " min.";
    } else {
        ss << "Rerouted! Changed roads: +" << diff.roads_added.size()
           << " / -" << diff.roads_removed.size()
           << " | Distance: " << std::showpos << std::fixed << std::setprecision(1) << diff.delta_distance_km << " km"
           << " | Time: " << diff.delta_time_min << " min.";
    }
    diff.summary = ss.str();
    return diff;
}

bool DynamicManager::apply_update(
    UpdateType type,
    const std::string& target_id,
    double num_value,
    bool bool_value,
    RouteDiff& diff_out,
    std::string& error_msg
) {
    // 1. Validation phase (Atomic guarantee: validate first before mutating)
    UndoRecord undo_rec;
    undo_rec.type = type;
    undo_rec.target_id = target_id;

    if (type == UpdateType::ChangePlaceCrowd) {
        ds::Place* p = graph_.get_place(target_id);
        if (!p) {
            error_msg = "Target place ID not found: " + target_id;
            return false;
        }
        if (num_value < 0.0 || num_value > 10.0) {
            error_msg = "Place crowd value must be between 0 and 10";
            return false;
        }
        undo_rec.old_num_val = p->crowd;
        undo_rec.description = "Revert crowd of " + p->name;
        // Apply
        p->crowd = static_cast<int>(num_value);
    } else {
        ds::Road* r = graph_.get_road(target_id);
        if (!r) {
            error_msg = "Target road ID not found: " + target_id;
            return false;
        }
        if (type == UpdateType::BlockRoad || type == UpdateType::UnblockRoad) {
            undo_rec.old_bool_val = r->blocked;
            undo_rec.description = (r->blocked ? "Re-block road " : "Unblock road ") + r->id;
            r->blocked = bool_value;
        } else if (type == UpdateType::ChangeTraffic) {
            if (num_value < 0.0 || num_value > 10.0) {
                error_msg = "Traffic level must be between 0 and 10";
                return false;
            }
            undo_rec.old_num_val = r->traffic;
            undo_rec.description = "Revert traffic on road " + r->id;
            r->traffic = num_value;
        } else if (type == UpdateType::ChangeRoadCrowd) {
            if (num_value < 0.0 || num_value > 10.0) {
                error_msg = "Road crowd level must be between 0 and 10";
                return false;
            }
            undo_rec.old_num_val = r->crowd;
            undo_rec.description = "Revert crowd on road " + r->id;
            r->crowd = num_value;
        }
    }

    // 2. Commit update to Stack and bump version
    undo_stack_.push(undo_rec);
    graph_.bump_version();

    // 3. Reachability check & Route recomputation
    if (has_active_query_) {
        // Collect targets for BFS check: end node + must-visit nodes
        ds::DynArray<int> bfs_targets;
        int end_idx = graph_.get_place_index(last_query_.end_id);
        if (end_idx >= 0) bfs_targets.push_back(end_idx);
        for (size_t i = 0; i < last_query_.must_visit.size(); ++i) {
            int mv = graph_.get_place_index(last_query_.must_visit[i]);
            if (mv >= 0) bfs_targets.push_back(mv);
        }

        int start_idx = graph_.get_place_index(last_query_.start_id);
        ds::DynArray<int> unreachable;
        bool all_reachable = check_reachability_bfs(start_idx, bfs_targets, &unreachable);

        RouteResult new_route;
        if (!all_reachable) {
            new_route.found = false;
            new_route.message = "Closure renders one or more mandatory stops unreachable.";
        } else {
            new_route = solve(graph_, last_query_).best;
        }

        diff_out = compute_diff(last_route_, new_route);
        last_route_ = new_route;
    }

    return true;
}

bool DynamicManager::undo(RouteDiff& diff_out, std::string& error_msg) {
    if (undo_stack_.empty()) {
        error_msg = "Undo stack is empty; no dynamic modifications to revert.";
        return false;
    }

    UndoRecord top = undo_stack_.pop();

    if (top.type == UpdateType::ChangePlaceCrowd) {
        ds::Place* p = graph_.get_place(top.target_id);
        if (p) p->crowd = static_cast<int>(top.old_num_val);
    } else {
        ds::Road* r = graph_.get_road(top.target_id);
        if (r) {
            if (top.type == UpdateType::BlockRoad || top.type == UpdateType::UnblockRoad) {
                r->blocked = top.old_bool_val;
            } else if (top.type == UpdateType::ChangeTraffic) {
                r->traffic = top.old_num_val;
            } else if (top.type == UpdateType::ChangeRoadCrowd) {
                r->crowd = top.old_num_val;
            }
        }
    }

    graph_.bump_version();

    if (has_active_query_) {
        RouteResult new_route;
        new_route = solve(graph_, last_query_).best;

        diff_out = compute_diff(last_route_, new_route);
        diff_out.summary = "Undo applied: " + top.description + " | " + diff_out.summary;
        last_route_ = new_route;
    }

    return true;
}

void DynamicManager::init_default_events() {
    event_queue_.clear();
    event_queue_.push(SimulatedEvent{
        "Ganesh Festival Aarti Rush",
        "Huge crowd spike at Dagadusheth Halwai Ganapati Temple corridor (e02 crowd -> 10)",
        UpdateType::ChangeRoadCrowd, "e02", 10.0
    });
    event_queue_.push(SimulatedEvent{
        "Road Work on Swargate Link",
        "Temporary pipeline excavation blocks road e10 (SW - SB)",
        UpdateType::BlockRoad, "e10", 1.0
    });
    event_queue_.push(SimulatedEvent{
        "Rain Congestion near Katraj Zoo",
        "Waterlogging increases traffic congestion on corridor e16 (PG - RZ traffic -> 9)",
        UpdateType::ChangeTraffic, "e16", 9.0
    });
    event_queue_.push(SimulatedEvent{
        "Heritage Walk Procession",
        "Shaniwar Wada to Lal Mahal street closed for cultural walk (e01 blocked)",
        UpdateType::BlockRoad, "e01", 1.0
    });
    event_queue_.push(SimulatedEvent{
        "VIP Green Corridor",
        "Pataleshwar Cave bypass cleared of all traffic (e04 traffic -> 1)",
        UpdateType::ChangeTraffic, "e04", 1.0
    });
}

bool DynamicManager::trigger_next_simulated_event(
    std::string& event_title,
    RouteDiff& diff_out,
    std::string& error_msg
) {
    if (event_queue_.empty()) {
        init_default_events();
    }

    SimulatedEvent ev = event_queue_.pop();
    event_title = ev.title + ": " + ev.description;

    bool bool_val = (ev.type == UpdateType::BlockRoad);
    return apply_update(ev.type, ev.target_id, ev.value, bool_val, diff_out, error_msg);
}

} // namespace planner

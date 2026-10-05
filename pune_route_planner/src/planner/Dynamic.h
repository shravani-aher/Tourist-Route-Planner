#pragma once

#include "../ds/Graph.h"
#include "../ds/Stack.h"
#include "../ds/Queue.h"
#include "../ds/UnionFind.h"
#include "Models.h"
#include <string>

namespace planner {

class DynamicManager {
private:
    ds::Graph& graph_;
    ds::Stack<UndoRecord> undo_stack_;
    ds::Queue<SimulatedEvent> event_queue_;

    bool has_active_query_ = false;
    RouteQuery last_query_;
    RouteResult last_route_;

public:
    explicit DynamicManager(ds::Graph& g);

    ds::Graph& graph() { return graph_; }
    const ds::Graph& graph() const { return graph_; }

    size_t undo_stack_depth() const { return undo_stack_.size(); }
    size_t pending_events_count() const { return event_queue_.size(); }

    void set_active_query(const RouteQuery& q, const RouteResult& initial_route);

    // BFS Reachability Check using ds::Queue
    bool check_reachability_bfs(
        int start_idx,
        const ds::DynArray<int>& targets,
        ds::DynArray<int>* unreachable_targets = nullptr
    ) const;

    // Disjoint Set Union analysis
    int compute_connected_components(ds::DynArray<int>* node_component_id = nullptr) const;

    // Atomic update operations
    bool apply_update(
        UpdateType type,
        const std::string& target_id,
        double num_value,
        bool bool_value,
        RouteDiff& diff_out,
        std::string& error_msg
    );

    // Undo operation
    bool undo(RouteDiff& diff_out, std::string& error_msg);

    // Simulated event queue management
    void init_default_events();
    bool trigger_next_simulated_event(std::string& event_title, RouteDiff& diff_out, std::string& error_msg);

    // Diff computation helper
    static RouteDiff compute_diff(const RouteResult& old_r, const RouteResult& new_r);

    const RouteResult& current_route() const { return last_route_; }
    bool has_active_query() const { return has_active_query_; }
    bool has_active_route() const { return has_active_query_ && last_route_.found; }
};

} // namespace planner

#pragma once

/**
 * @file Graph.h
 * @brief Adjacency List Graph representation with shared mutable Road records.
 *
 * DATA STRUCTURE CONCEPT:
 * - Graph ADT represented as an Adjacency List:
 *     adj_[u] contains a DynArray of outgoing Arc{to, roadId}.
 * - Shared Mutable Road Table:
 *     Road properties (distance, base time, traffic, scenic, crowd, blocked status)
 *     are stored in a single hash table roads_[roadId].
 *     For an undirected edge (u, v), two directed arcs (u -> v) and (v -> u) are stored
 *     in the adjacency lists, but both point to the SAME road record in roads_.
 *     When a road is blocked or its crowd/traffic is updated, a single mutation to roads_[roadId]
 *     instantly and consistently updates traversals in BOTH directions in O(1) time.
 * - Parallel Roads:
 *     Multiple distinct roads can connect the same pair of nodes (u, v), differentiated
 *     by unique road IDs. Paths store both node indices AND road IDs traversed.
 * - Time Complexity:
 *     - Neighbor scanning: O(deg(u)) per node u, which is optimal for Dijkstra / BFS.
 *     - Space Complexity: O(V + E).
 */

#include "DynArray.h"
#include "HashMap.h"
#include <string>
#include <vector>

namespace ds {

struct Place {
    std::string id;
    std::string name;
    DynArray<std::string> categories;
    int visit_minutes = 0;
    int crowd = 0;
    std::string category;
    int open_hour = 0, close_hour = 24;
    int crowd_weekday[24] = {0}, crowd_weekend[24] = {0};
    bool metrics_estimated = false;
    double x = 0.0;
    double y = 0.0;
    int index = -1;
    bool attraction = true;

    bool has_category(const std::string& cat) const {
        for (const auto& c : categories) {
            if (c == cat) return true;
        }
        return false;
    }
};

struct Road {
    std::string id;
    std::string u;
    std::string v;
    int u_idx = -1;
    int v_idx = -1;
    bool directed = false;
    double distance_km = 0.0;
    double base_time_min = 0.0;
    double scenic = 0.0;   // 0 .. 10
    double crowd = 0.0;    // 0 .. 10
    double traffic = 0.0;  // 0 .. 10
    bool blocked = false;
    std::string osm_way;
    std::string name;
    std::string highway;
    unsigned char crowd_tbl[2][24] = {{0}};
    bool has_crowd_tbl = false;

    // Derived effective travel time t = base_time * (1 + traffic / 10.0)
    double effective_time_min() const {
        return base_time_min * (1.0 + traffic / 10.0);
    }
};

struct Arc {
    int to = -1;
    std::string roadId;
};

class Graph {
private:
    DynArray<Place> places_;
    HashMap<std::string, int> place_to_index_;
    HashMap<std::string, Road> roads_;
    DynArray<std::string> road_ids_;
    DynArray<DynArray<Arc>> adj_;
    uint64_t version_ = 0;

public:
    Graph() = default;
    bool real_data = false;
    bool metrics_estimated = false;
    struct Turn { std::string from_way, to_way; bool only; };
    HashMap<std::string, DynArray<Turn>> turns;
    bool turn_allowed(int via, const std::string& from, const std::string& to) const {
        const auto* rules = turns.find(get_place(via).id);
        if (!rules || from.empty()) return true;
        for (const auto& r : *rules) if (r.from_way == from && ((r.only && r.to_way != to) || (!r.only && r.to_way == to))) return false;
        return true;
    }
    void add_alias(const std::string& id, int idx) { place_to_index_.insert(id, idx); }

    uint64_t version() const noexcept { return version_; }
    void bump_version() noexcept { ++version_; }

    size_t num_places() const noexcept { return places_.size(); }
    size_t num_roads() const noexcept { return road_ids_.size(); }

    int add_place(Place p) {
        int idx = static_cast<int>(places_.size());
        p.index = idx;
        std::string id = p.id;
        places_.push_back(std::move(p));
        place_to_index_.insert(id, idx);
        adj_.push_back(DynArray<Arc>());
        bump_version();
        return idx;
    }

    bool has_place(const std::string& id) const {
        return place_to_index_.contains(id);
    }

    int get_place_index(const std::string& id) const {
        const int* idx = place_to_index_.find(id);
        return idx ? *idx : -1;
    }

    const Place& get_place(int idx) const {
        return places_[idx];
    }

    Place& get_place(int idx) {
        return places_[idx];
    }

    const Place* get_place(const std::string& id) const {
        int idx = get_place_index(id);
        return idx >= 0 ? &places_[idx] : nullptr;
    }

    Place* get_place(const std::string& id) {
        int idx = get_place_index(id);
        return idx >= 0 ? &places_[idx] : nullptr;
    }

    const DynArray<Place>& all_places() const {
        return places_;
    }

    bool add_road(Road r) {
        int u_idx = get_place_index(r.u);
        int v_idx = get_place_index(r.v);
        if (u_idx < 0 || v_idx < 0) return false;

        r.u_idx = u_idx;
        r.v_idx = v_idx;
        std::string r_id = r.id;

        roads_.insert_or_assign(r_id, r);
        road_ids_.push_back(r_id);

        adj_[u_idx].push_back(Arc{v_idx, r_id});
        if (!r.directed) {
            adj_[v_idx].push_back(Arc{u_idx, r_id});
        }

        bump_version();
        return true;
    }

    const Road* get_road(const std::string& id) const {
        return roads_.find(id);
    }

    Road* get_road(const std::string& id) {
        return roads_.find(id);
    }

    const DynArray<Arc>& get_outgoing_arcs(int node_idx) const {
        return adj_[node_idx];
    }

    const DynArray<std::string>& all_road_ids() const {
        return road_ids_;
    }

    HashMap<std::string, Road>& road_table() {
        return roads_;
    }

    const HashMap<std::string, Road>& road_table() const {
        return roads_;
    }

    void clear() {
        places_.clear();
        place_to_index_.clear();
        roads_.clear();
        road_ids_.clear();
        adj_.clear();
        version_ = 0;
    }
};

} // namespace ds

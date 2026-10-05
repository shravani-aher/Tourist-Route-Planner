#pragma once

#include "../ds/DynArray.h"
#include <string>
#include <vector>

namespace planner {

enum class Mode {
    Balanced,
    Shortest,
    Fastest,
    Scenic,
    LeastCrowded
};

inline const char* mode_to_string(Mode m) {
    switch (m) {
        case Mode::Balanced: return "balanced";
        case Mode::Shortest: return "shortest";
        case Mode::Fastest: return "fastest";
        case Mode::Scenic: return "scenic";
        case Mode::LeastCrowded: return "least_crowded";
    }
    return "balanced";
}

inline Mode string_to_mode(const std::string& s) {
    if (s == "shortest") return Mode::Shortest;
    if (s == "fastest") return Mode::Fastest;
    if (s == "scenic") return Mode::Scenic;
    if (s == "least_crowded") return Mode::LeastCrowded;
    return Mode::Balanced;
}

struct PreferenceWeights {
    double wd = 0.20; // distance weight
    double wt = 0.30; // time weight
    double ws = 0.20; // scenic weight
    double wc = 0.15; // crowd weight
    double wp = 0.15; // interest match weight

    bool validate_and_normalize(std::string& error_msg);
};

struct RouteQuery {
    std::string start_id;
    std::string end_id;
    Mode primary_mode = Mode::Balanced;
    PreferenceWeights weights;
    ds::DynArray<std::string> interests;
    ds::DynArray<std::string> avoid;
    ds::DynArray<std::string> must_visit;
    double max_time_min = -1.0;     // <= 0 means unlimited
    double max_distance_km = -1.0; // <= 0 means unlimited
    int k_alternatives = 5;
};

struct RouteLeg {
    std::string road_id;
    int from_idx = -1;
    int to_idx = -1;
    std::string from_id;
    std::string to_id;
    double distance_km = 0.0;
    double travel_time_min = 0.0;
    double scenic = 0.0;
    double crowd = 0.0;
    double traffic = 0.0;
};

struct PlannedStop {
    std::string place_id;
    std::string name;
    int visit_minutes = 0;
    bool is_must_visit = false;
    bool is_start = false;
    bool is_end = false;
};

struct RouteResult {
    bool found = false;
    std::string message;
    std::string mode_label; // e.g. "balanced", "shortest", "fastest", etc.

    ds::DynArray<int> node_path;       // sequence of node indices
    ds::DynArray<std::string> road_path;   // sequence of road IDs
    ds::DynArray<RouteLeg> legs;
    ds::DynArray<PlannedStop> stops;

    double total_distance_km = 0.0;
    double total_travel_time_min = 0.0;
    double total_visit_time_min = 0.0;
    double total_time_min = 0.0; // travel + visit
    double avg_scenic = 0.0;
    double avg_crowd = 0.0;
    double balanced_cost = 0.0;
    double demo_index = 0.0;     // 100 / (1 + balanced_cost)

    // For visualization
    ds::DynArray<int> settled_order; // sequence of node indices settled by Dijkstra
};

struct RouteDiff {
    bool has_old = false;
    bool has_new = false;
    double delta_distance_km = 0.0;
    double delta_time_min = 0.0;
    double delta_balanced_cost = 0.0;
    ds::DynArray<std::string> roads_added;
    ds::DynArray<std::string> roads_removed;
    std::string summary;
};

enum class UpdateType {
    BlockRoad,
    UnblockRoad,
    ChangeTraffic,
    ChangeRoadCrowd,
    ChangePlaceCrowd
};

struct UndoRecord {
    UpdateType type;
    std::string target_id; // road_id or place_id
    double old_num_val = 0.0;
    bool old_bool_val = false;
    std::string description;
};

struct SimulatedEvent {
    std::string title;
    std::string description;
    UpdateType type;
    std::string target_id;
    double value = 0.0;
};

struct SystemStats {
    size_t heap_size = 0;
    size_t hash_load_factor_pct = 0;
    size_t hash_bucket_count = 0;
    size_t nodes_visited_last_run = 0;
    size_t undo_stack_depth = 0;
    size_t pending_events_count = 0;
    int connected_components = 1;
};

} // namespace planner

#pragma once
#include "../ds/Graph.h"
namespace planner {
// Departure time of the active query. Every handler holds the server state mutex, so a plain global is safe.
struct TimeContext { int hour = 12; bool weekend = false; };
inline TimeContext& time_context() { static TimeContext c; return c; } // guarded by the server state mutex
inline double road_crowd(const ds::Road& r) {
    if (!r.has_crowd_tbl) return r.crowd;
    const auto& c = time_context();
    double modelled = r.crowd_tbl[c.weekend ? 1 : 0][c.hour];
    return r.crowd > modelled ? r.crowd : modelled; // a user-entered crowd spike overrides the model upward
}
}

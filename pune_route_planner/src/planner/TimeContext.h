#pragma once
#include "../ds/Graph.h"
namespace planner {
// Per-request departure time. The server serializes requests, so a thread_local is safe.
struct TimeContext { int hour = 12; bool weekend = false; };
inline TimeContext& time_context() { static thread_local TimeContext c; return c; }
inline double road_crowd(const ds::Road& r) {
    if (!r.has_crowd_tbl) return r.crowd;
    const auto& c = time_context();
    return r.crowd_tbl[c.weekend ? 1 : 0][c.hour];
}
}

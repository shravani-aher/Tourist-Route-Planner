# Pune Tourist Route Planner: algorithm and test report

## Problem
Given places (nodes) and roads (edges), produce personalized tourist routes, not only the shortest. Factors: distance, time, scenic value, crowd, user preference. Outputs: shortest, fastest, most scenic, least crowded, balanced; tours through must-visit stops; re-planning when roads are blocked.

## Data
OpenStreetMap Geofabrik Western India extract (snapshot 2026-10-04, SHA256 in the dataset manifest), bounded to lat 18.485-18.565, lon 73.825-73.915: 32,186 vertices, 35,382 directed road segments, 25 node turn restrictions. 18 attractions with category, visit minutes, opening hours and hourly crowd curves. Scenic scores, crowd and hours are estimates (docs/models.md). Live traffic is not used.

## Data structures (all hand-written in src/ds)
| Structure | Used for | Cost |
|---|---|---|
| DynArray | paths, adjacency lists, candidate pools | amortized O(1) append |
| HashMap | place and road lookup by id, turn rules, edge penalties | expected O(1) |
| MinHeap | Dijkstra frontier | O(log n) push/pop |
| Trie | attraction name autocomplete | O(prefix length) |
| Stack | undo of what-if edits | O(1) |
| Queue | simulated event queue (fixture mode) | O(1) |
| UnionFind | connected components, reachability checks | near O(1) amortized |
| merge sort | ranking candidate routes and optional stops | O(n log n) |

## Algorithms
- **Dijkstra over arrival arcs.** State is the directed arc used to arrive, so turn restrictions depend on the incoming way. States are at most one per arc (about 70k). Time O(A log A) per query; measured about 40 ms per search here.
- **Mode costs.** Shortest: distance. Fastest: time. Scenic: distance x (1.1 - scenic/10). Least crowded: time x (0.1 + crowd/10). Balanced: weighted sum of distance, time, scenic loss and crowd. Crowd depends on departure hour and weekday/weekend.
- **Alternatives.** Dijkstra with compounding edge penalties on roads already used, up to a bounded number of reruns, then greedy selection of routes with road overlap below 0.70. Heuristic, not exact k-shortest paths.
- **Tours.** Nearest-neighbour order, 2-opt refinement, then greedy insertion of optional attractions by benefit per minute (top 6) under the time budget. Each stop is scheduled from the departure hour with opening-hour waits; a mandatory stop that would be closed rejects the plan. Leg results are cached per plan. Heuristic, not an exact TSP solver.
- **Dynamic updates.** Block/unblock, traffic and road-crowd edits mutate a shared graph; the active route is re-solved and diffed against the previous one. Undo pops the stack.

## Tests
- C++ unit and fixture suite (data structures, scoring, alternatives, tours, JSON).
- Turn-restriction regressions.
- Importer policy tests and enrichment tests (Python).
- Real-graph HTTP suite: provenance, route continuity, all five modes and their ordering invariants, alternatives distinctness, scheduled tours and closed-stop rejection, block/undo round trip, malformed input, content-type, concurrency, schema validation.
- Headless-browser tests: responsive widths, route cards, tour itinerary, closed-stop message, block/undo, GPX export, share link.
CI runs Debug, Release, ASan/UBSan, browser tests, a Windows build and a Linux package artifact; I have not seen CI results on GitHub.

## Limits
Estimated metrics, 18 attractions, bounded area, no live data, no turn-by-turn navigation, shared edit state per server, heuristic alternatives and tours, no ThreadSanitizer or fuzzing. Windows and macOS runtime and real-device behaviour are unverified.

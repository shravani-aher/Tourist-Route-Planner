# Pune Tourist Route Planner

Local C++17 driving-route coursework app, using custom adjacency lists, hash map,
min-heap, dynamic arrays, Dijkstra, sorting, queue, stack, trie and union-find.

## M2 offline road data

The default app uses an OSM street graph, not synthetic attraction-to-attraction
corridors. Geographic vertices and street segments are separate from a catalog
of nine map-sourced attractions. Each attraction routes to a mapped street vertex
near its boundary or trail approach. These are **not field-verified entrances or
parking**, and the approach gap is shown separately and excluded from driving totals.
The zoo is not included in this initial catalog.

All five modes (shortest, fastest, scenic, least crowded, balanced) work on the estimated metrics, with departure `hour`/`weekend` inputs. `k` (1-5) returns distinct alternatives (road overlap under 0.70) found by bounded edge-penalty reruns; they are heuristic, not exact k-shortest paths. Hard limits are checked on that objective-optimal route, not an exhaustive constrained-path search. Travel time
is a documented class-based free-flow model capped by parseable OSM speed limits,
not live traffic or an arrival promise. Live traffic and live crowd are unavailable. Scenic scores (per road segment, from OSM parks, water and heritage proximity), per-attraction hourly crowd curves, visit durations and opening hours are **model/curated estimates** (schema v3), labelled as such in the data and UI, never presented as live or official. Tours (`mustVisit`, optional `maxTimeMin` to add extra attractions) are scheduled from the departure `hour`: each stop gets arrival/departure times, waits for estimated opening hours, and the request is rejected if a mandatory stop would be closed. Visit times and hours are estimates.
This is not turn-by-turn navigation.

## Estimated metrics (schema v3)

`tools/enrich_metrics.py` adds scenic scores and the 18-attraction catalog metadata from
`tools/attractions_meta.json` to the base import. Rerun after editing the metadata:

```sh
python3 pune_route_planner/tools/enrich_metrics.py base_v2.json western.osm.pbf pune_route_planner/data/pune_driving.json
```

Hours and visit times are planning estimates, not verified with venues. See docs/models.md.

## Build and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure --no-tests=error
./build/pune_route_planner/pune_route_planner --data pune_route_planner/data/pune_driving.json --web pune_route_planner/web_real
```

Open http://127.0.0.1:8080. On Windows the executable path depends on the generator.

```sh
cmake --install build --prefix release
cd release
./pune_route_planner
```

The installed release includes real data and the plain JS real-data UI, never
synthetic fixtures or simulation UI. No fallback occurs when data is missing,
invalid or synthetic. Legacy classroom tests require `--test-fixture` explicitly.

## Reproduce the import

Python 3.9+ and pinned `osmium==4.1.1` (pyosmium/libosmium) parse PBF offline.
The bounded area is latitude 18.485-18.565, longitude 73.825-73.915. Segments
crossing the bounds are omitted, so a route that needs to leave the area may fail.

```sh
python3 -m pip install -r pune_route_planner/tools/requirements.txt
curl -L https://download.geofabrik.de/asia/india/western-zone-261004.osm.pbf -o western.osm.pbf
python3 pune_route_planner/tools/import_osm.py western.osm.pbf pune_route_planner/data/pune_driving.json --source-url https://download.geofabrik.de/asia/india/western-zone-261004.osm.pbf
```

Snapshot: 2026-10-04T20:20:21Z. Source SHA256:
`b4bc6943b61186e810c26ba136120c8472839f52e588c210ce2a2ce399b2fad7`.
The generated file carries its source, snapshot, profile, bounding box, time
model, attribution, license and conservative exclusions. Atomic file replacement
and staged server validation prevent partial imports becoming active graphs.

One-way, reverse-one-way and default roundabout/motorway direction rules are
preserved. Specific motorcar access overrides generic access. Private,
destination-only, conditional and barrier paths are conservatively omitted.
Node-based no/only turns use arrival-arc state-expanded Dijkstra. Same-way no-U-turn rules are conservatively applied to that entire from/to-way pair, which may also remove legal continuations. Complex via-way
and conditional restrictions cause their approach ways to be excluded, rather
than silently allowing illegal turns. This can remove valid routes. Missing OSM
restrictions and real-world access cannot be guaranteed by this snapshot.

## Data license and sources

© OpenStreetMap contributors. The derived database is available under ODbL 1.0:
https://www.openstreetmap.org/copyright
Source extract: https://download.geofabrik.de/asia/india/western-zone.html
Road snapshot is in `pune_route_planner/data/pune_driving.json`; catalog OSM
feature IDs and snap provenance are embedded in that database.

No Nominatim, public OSRM, tile service, live feed or paid provider is needed at
runtime. The UI draws the actual offline street geometry with geographic aspect
ratio preserved, avoiding straight synthetic attraction corridors.

## Tests and limits

Legacy data-structure/HTTP fixtures remain only in tests. New tests cover
arrival-state turn restrictions, directed traversal, real graph route continuity,
available/unavailable metrics, and production rejection of fixtures/missing data.
Headless Chrome checks production UI routing and widths 320, 390, 768 and 1440.
CI runs Debug, Release, ASan/UBSan, HTTP and browser checks. A passing local test is
not a claim that GitHub CI has passed. ASan quarantine is set to 16 MiB in CI to fit constrained runners (checks and leak detection remain enabled). Physical devices, Windows/macOS, field
navigation, exhaustive access-tag semantics, screen readers and load testing
remain unverified. Local state is shared between clients; this is not a hosted
multi-user service.

## Live edits (blocked roads, traffic, crowd spikes)

`POST /api/update` (`block`, `unblock`, `traffic`, `road_crowd`) re-plans the active route and returns a diff (roads added/removed, distance and time change); `POST /api/undo` reverts the last edit. Edits are user-entered what-ifs, not live data. State is shared by every client of one running server (single-user local app); per-session isolation is not implemented. `/api/simulate` stays disabled on the real graph.

## Docker

```sh
docker build -t pune-planner .
docker run --rm -p 8080:8080 pune-planner
```

The container listens on 0.0.0.0 via `--host`. There is no authentication and edit state is shared, so do not expose it publicly. (Dockerfile not built in CI yet.)

## Export and sharing

The UI downloads the selected route as GPX (track plus stop waypoints) and copies a share link that restores the same query (`#s=..&e=..&m=..&t=..`). Share links encode the query, not edits.

## Performance and hardening

Route search runs on integer-indexed flattened arcs (no string hashing in the loop): a 5-mode request on the 32k-vertex graph takes about 0.2 s here (was 1-2 s). The browser loads `/api/map` (3.3 MB, geometry only) instead of `/api/graph` (17 MB). The server is same-origin only (no CORS headers), POST bodies must be `application/json` (415 otherwise), bodies are capped at 64 KiB, and concurrent requests are serialized by one state mutex (tested with 8 parallel clients). ThreadSanitizer and fuzzing are not set up.

#include <iostream>
#include <cassert>
#include <cmath>
#include <string>
#include <fstream>
#include <sstream>

#include "../src/ds/DynArray.h"
#include "../src/ds/MinHeap.h"
#include "../src/ds/HashMap.h"
#include "../src/ds/Stack.h"
#include "../src/ds/Queue.h"
#include "../src/ds/Sort.h"
#include "../src/ds/UnionFind.h"
#include "../src/ds/Trie.h"
#include "../src/ds/Graph.h"

#include "../src/planner/Models.h"
#include "../src/planner/Scoring.h"
#include "../src/planner/Dijkstra.h"
#include "../src/planner/Alternatives.h"
#include "../src/planner/Tour.h"
#include "../src/planner/Dynamic.h"
#include "../src/util/Json.h"

static bool approx_equal(double a, double b, double eps = 1e-4) {
    return std::abs(a - b) <= eps;
}

// Helper to populate graph from JSON file
static bool load_demo_graph(const std::string& path, ds::Graph& g) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::stringstream buf;
    buf << f.rdbuf();

    std::string err;
    util::JsonValue root = util::JsonValue::parse(buf.str(), &err);
    if (!err.empty()) return false;

    const auto& places_arr = root["places"].as_array();
    for (const auto& pv : places_arr) {
        ds::Place p;
        p.id = pv["id"].as_string();
        p.name = pv["name"].as_string();
        p.visit_minutes = pv["visit_minutes_demo"].as_int();
        p.crowd = pv["crowd_demo"].as_int();
        p.x = pv["x_schematic"].as_double();
        p.y = pv["y_schematic"].as_double();
        for (const auto& cat : pv["categories"].as_array()) {
            p.categories.push_back(cat.as_string());
        }
        g.add_place(p);
    }

    const auto& roads_arr = root["roads"].as_array();
    for (const auto& rv : roads_arr) {
        ds::Road r;
        r.id = rv["id"].as_string();
        r.u = rv["u"].as_string();
        r.v = rv["v"].as_string();
        r.directed = rv["directed"].as_bool();
        r.distance_km = rv["distance_km_demo"].as_double();
        r.base_time_min = rv["base_time_min_demo"].as_double();
        r.scenic = rv["scenic_demo"].as_double();
        r.crowd = rv["crowd_demo"].as_double();
        r.traffic = rv["traffic_demo"].as_double();
        r.blocked = rv["blocked"].as_bool();
        g.add_road(r);
    }
    return true;
}

void test_dyn_array() {
    std::cout << "[RUN] test_dyn_array..." << std::endl;
    ds::DynArray<int> arr;
    assert(arr.size() == 0);
    assert(arr.empty());

    for (int i = 0; i < 50; ++i) {
        arr.push_back(i * 2);
    }
    assert(arr.size() == 50);
    assert(arr.capacity() >= 50);
    assert(arr[0] == 0);
    assert(arr[49] == 98);
    assert(arr.front() == 0);
    assert(arr.back() == 98);

    arr.pop_back();
    assert(arr.size() == 49);
    assert(arr.back() == 96);

    // Copy and Move
    ds::DynArray<int> copy = arr;
    assert(copy.size() == 49);
    assert(copy[10] == 20);

    ds::DynArray<int> moved = std::move(copy);
    assert(moved.size() == 49);
    assert(copy.size() == 0);
    std::cout << "  -> PASSED" << std::endl;
}

void test_min_heap() {
    std::cout << "[RUN] test_min_heap (order + decrease-key)..." << std::endl;
    ds::MinHeap<double, int> heap(20);
    assert(heap.empty());

    heap.push(1, 10.5);
    heap.push(2, 4.2);
    heap.push(3, 15.0);
    heap.push(4, 1.8);
    heap.push(5, 7.3);

    assert(heap.size() == 5);
    assert(heap.peek().id == 4);
    assert(approx_equal(heap.peek().priority, 1.8));

    // Test decrease-key: lower priority of id 3 from 15.0 to 0.5
    bool dk_res = heap.decrease_key(3, 0.5);
    assert(dk_res);
    assert(heap.peek().id == 3);
    assert(approx_equal(heap.peek().priority, 0.5));

    // Sequential pop order verification
    auto top1 = heap.pop();
    assert(top1.id == 3 && approx_equal(top1.priority, 0.5));

    auto top2 = heap.pop();
    assert(top2.id == 4 && approx_equal(top2.priority, 1.8));

    auto top3 = heap.pop();
    assert(top3.id == 2 && approx_equal(top3.priority, 4.2));

    auto top4 = heap.pop();
    assert(top4.id == 5 && approx_equal(top4.priority, 7.3));

    auto top5 = heap.pop();
    assert(top5.id == 1 && approx_equal(top5.priority, 10.5));

    assert(heap.empty());
    std::cout << "  -> PASSED" << std::endl;
}

void test_hash_map() {
    std::cout << "[RUN] test_hash_map (insert, lookup, collision, rehash)..." << std::endl;
    ds::HashMap<std::string, int> map(4); // small initial bucket count to force rehashing
    assert(map.empty());

    map.insert("SW", 100);
    map.insert("LM", 200);
    map.insert("DG", 300);
    assert(map.size() == 3);

    assert(map.contains("SW"));
    assert(*map.find("SW") == 100);
    assert(*map.find("LM") == 200);
    assert(*map.find("DG") == 300);
    assert(!map.contains("UNKNOWN"));

    // Add many entries to trigger rehash
    for (int i = 0; i < 30; ++i) {
        map.insert("key_" + std::to_string(i), i * 10);
    }
    assert(map.size() == 33);
    assert(map.bucket_count() > 4);

    // Verify all keys remain intact after rehashing
    assert(*map.find("SW") == 100);
    for (int i = 0; i < 30; ++i) {
        std::string k = "key_" + std::to_string(i);
        assert(map.contains(k));
        assert(*map.find(k) == i * 10);
    }

    // Erase test
    bool erased = map.erase("key_5");
    assert(erased);
    assert(!map.contains("key_5"));
    assert(map.size() == 32);

    // operator[] test
    map["brand_new"] = 999;
    assert(*map.find("brand_new") == 999);
    map["brand_new"] = 1000;
    assert(map["brand_new"] == 1000);
    std::cout << "  -> PASSED" << std::endl;
}

void test_stack_queue() {
    std::cout << "[RUN] test_stack_queue..." << std::endl;
    // Stack LIFO
    ds::Stack<std::string> st;
    assert(st.empty());
    st.push("first");
    st.push("second");
    st.push("third");
    assert(st.size() == 3);
    assert(st.top() == "third");
    assert(st.pop() == "third");
    assert(st.pop() == "second");
    assert(st.pop() == "first");
    assert(st.empty());

    // Queue FIFO circular buffer
    ds::Queue<int> q(4);
    assert(q.empty());
    q.push(10);
    q.push(20);
    q.push(30);
    assert(q.size() == 3);
    assert(q.front() == 10);
    assert(q.pop() == 10);
    assert(q.pop() == 20);

    // Push more to test wraparound and resizing
    for (int i = 100; i < 120; ++i) {
        q.push(i);
    }
    assert(q.pop() == 30);
    assert(q.pop() == 100);
    assert(q.size() == 19);
    std::cout << "  -> PASSED" << std::endl;
}

void test_sort_stability() {
    std::cout << "[RUN] test_sort_stability (Merge Sort)..." << std::endl;
    struct Item {
        int key;
        int original_pos;
    };

    ds::DynArray<Item> items;
    items.push_back(Item{5, 0});
    items.push_back(Item{2, 1});
    items.push_back(Item{5, 2});
    items.push_back(Item{1, 3});
    items.push_back(Item{5, 4});
    items.push_back(Item{2, 5});

    ds::merge_sort(items, [](const Item& a, const Item& b) {
        return a.key < b.key;
    });

    // Keys must be in non-decreasing order
    for (size_t i = 1; i < items.size(); ++i) {
        assert(items[i - 1].key <= items[i].key);
    }

    // Check stability: for key 5, original positions must be 0, 2, 4 in that exact order
    ds::DynArray<int> fives_pos;
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].key == 5) fives_pos.push_back(items[i].original_pos);
    }
    assert(fives_pos.size() == 3);
    assert(fives_pos[0] == 0);
    assert(fives_pos[1] == 2);
    assert(fives_pos[2] == 4);

    // Tiny list insertion sort test
    ds::DynArray<int> tiny = {9, 3, 7, 1, 4};
    ds::insertion_sort(tiny, [](int a, int b) { return a < b; });
    assert(tiny[0] == 1 && tiny[1] == 3 && tiny[2] == 4 && tiny[3] == 7 && tiny[4] == 9);
    std::cout << "  -> PASSED" << std::endl;
}

void test_union_find_and_trie() {
    std::cout << "[RUN] test_union_find_and_trie..." << std::endl;
    // Union-Find
    ds::UnionFind uf(5);
    assert(uf.component_count() == 5);
    uf.unite(0, 1);
    uf.unite(1, 2);
    assert(uf.connected(0, 2));
    assert(!uf.connected(0, 3));
    assert(uf.component_count() == 3);

    // Trie autocomplete
    ds::Trie trie;
    trie.insert("shaniwar wada", "SW", "Shaniwar Wada");
    trie.insert("sarasbaug", "SB", "Sarasbaug");
    trie.insert("parvati hill", "PH", "Parvati Hill");
    trie.insert("pataleshwar cave temple", "PT", "Pataleshwar Cave Temple");

    auto matches = trie.search_prefix("shan");
    assert(matches.size() == 1);
    assert(matches[0].place_id == "SW");

    auto p_matches = trie.search_prefix("pa");
    assert(p_matches.size() == 2); // Parvati Hill & Pataleshwar Cave Temple
    std::cout << "  -> PASSED" << std::endl;
}

void test_dijkstra_demo_benchmarks(const ds::Graph& g) {
    std::cout << "[RUN] test_dijkstra_demo_benchmarks (SW to SB shortest vs fastest)..." << std::endl;
    int sw_idx = g.get_place_index("SW");
    int sb_idx = g.get_place_index("SB");
    assert(sw_idx >= 0 && sb_idx >= 0);

    // SW to SB SHORTEST: Prompt specifies 3.2 km via e10
    planner::Dijkstra::Options shortest_opts;
    shortest_opts.mode = planner::Mode::Shortest;
    planner::RouteResult shortest_res = planner::Dijkstra::find_path(g, sw_idx, sb_idx, shortest_opts);

    assert(shortest_res.found);
    std::cout << "  SW->SB Shortest distance: " << shortest_res.total_distance_km << " km (Expected: 3.2 km)" << std::endl;
    assert(approx_equal(shortest_res.total_distance_km, 3.2));
    assert(shortest_res.road_path.size() == 1);
    assert(shortest_res.road_path[0] == "e10");

    // SW to SB FASTEST: Prompt specifies 22.8 min via e04+e11 with default traffic factor
    // e04 (SW->PT): base 7, tr 2 -> 7 * (1 + 0.2) = 8.4 min
    // e11 (PT->SB): base 12, tr 2 -> 12 * (1 + 0.2) = 14.4 min
    // Total = 8.4 + 14.4 = 22.8 min!
    planner::Dijkstra::Options fastest_opts;
    fastest_opts.mode = planner::Mode::Fastest;
    planner::RouteResult fastest_res = planner::Dijkstra::find_path(g, sw_idx, sb_idx, fastest_opts);

    assert(fastest_res.found);
    std::cout << "  SW->SB Fastest time: " << fastest_res.total_travel_time_min << " min (Expected: 22.8 min)" << std::endl;
    assert(approx_equal(fastest_res.total_travel_time_min, 22.8));
    assert(fastest_res.road_path.size() == 2);
    assert(fastest_res.road_path[0] == "e04");
    assert(fastest_res.road_path[1] == "e11");

    // Start == End special case
    planner::RouteResult zero_res = planner::Dijkstra::find_path(g, sw_idx, sw_idx, shortest_opts);
    assert(zero_res.found);
    assert(zero_res.total_distance_km == 0.0);
    assert(zero_res.road_path.empty());
    assert(zero_res.node_path.size() == 1 && zero_res.node_path[0] == sw_idx);
    std::cout << "  -> PASSED" << std::endl;
}

void test_dynamic_updates_undo_and_bfs(ds::Graph& g) {
    std::cout << "[RUN] test_dynamic_updates_undo_and_bfs..." << std::endl;
    planner::DynamicManager dm(g);

    planner::RouteQuery q;
    q.start_id = "SW";
    q.end_id = "SB";
    q.primary_mode = planner::Mode::Shortest;

    int sw_idx = g.get_place_index("SW");
    int sb_idx = g.get_place_index("SB");

    planner::Dijkstra::Options opts;
    opts.mode = planner::Mode::Shortest;
    planner::RouteResult init_route = planner::Dijkstra::find_path(g, sw_idx, sb_idx, opts);
    assert(init_route.found);
    assert(init_route.road_path[0] == "e10");

    dm.set_active_query(q, init_route);

    // 1. Block road e10
    planner::RouteDiff diff;
    std::string err;
    bool ok = dm.apply_update(planner::UpdateType::BlockRoad, "e10", 0.0, true, diff, err);
    assert(ok);
    assert(dm.undo_stack_depth() == 1);
    assert(diff.has_new);
    std::cout << "  After blocking e10: " << diff.summary << std::endl;

    // After blocking e10, the shortest route should no longer use e10
    assert(dm.current_route().found);
    for (size_t i = 0; i < dm.current_route().road_path.size(); ++i) {
        assert(dm.current_route().road_path[i] != "e10");
    }
    assert(dm.current_route().total_distance_km > 3.2);

    // 2. Undo the block
    ok = dm.undo(diff, err);
    assert(ok);
    assert(dm.undo_stack_depth() == 0);
    std::cout << "  After Undo: " << diff.summary << std::endl;

    // Route should be restored back to e10 with 3.2 km
    assert(dm.current_route().found);
    assert(approx_equal(dm.current_route().total_distance_km, 3.2));
    assert(dm.current_route().road_path[0] == "e10");

    // 3. BFS reachability test
    ds::DynArray<int> targets;
    targets.push_back(sb_idx);
    targets.push_back(g.get_place_index("RZ"));
    assert(dm.check_reachability_bfs(sw_idx, targets));

    // Disconnect Katraj Zoo RZ by blocking e16, e17, e18
    dm.apply_update(planner::UpdateType::BlockRoad, "e16", 0.0, true, diff, err);
    dm.apply_update(planner::UpdateType::BlockRoad, "e17", 0.0, true, diff, err);
    dm.apply_update(planner::UpdateType::BlockRoad, "e18", 0.0, true, diff, err);

    ds::DynArray<int> unreachable;
    bool reachable = dm.check_reachability_bfs(sw_idx, targets, &unreachable);
    assert(!reachable);
    assert(unreachable.size() == 1 && unreachable[0] == g.get_place_index("RZ"));

    // UnionFind component count should now be 2
    int components = dm.compute_connected_components();
    std::cout << "  Components after isolating RZ: " << components << " (Expected: 2)" << std::endl;
    assert(components == 2);

    // Undo the 3 blocks
    dm.undo(diff, err);
    dm.undo(diff, err);
    dm.undo(diff, err);
    assert(dm.compute_connected_components() == 1);
    std::cout << "  -> PASSED" << std::endl;
}

void test_tour_planner(const ds::Graph& g) {
    std::cout << "[RUN] test_tour_planner (must-visit, conflict, budget)..." << std::endl;
    planner::RouteQuery q;
    q.start_id = "SW";
    q.end_id = "RZ";
    q.must_visit.push_back("DG"); // Dagadusheth Ganapati
    q.must_visit.push_back("PH"); // Parvati Hill

    // Test 1: Conflict detection
    q.avoid.push_back("temple"); // DG and PH have 'temple' category
    std::string err;
    planner::RouteResult conf_res = planner::Tour::plan_tour(g, q, err);
    assert(!conf_res.found);
    std::cout << "  Conflict detected successfully: " << err << std::endl;
    assert(err.find("Conflict detected") != std::string::npos);

    // Remove conflict
    q.avoid.clear();
    planner::RouteResult tour_res = planner::Tour::plan_tour(g, q, err);
    assert(tour_res.found);
    std::cout << "  Tour planned with " << tour_res.stops.size() << " stops. Total time: "
              << tour_res.total_time_min << " min (" << tour_res.total_travel_time_min
              << " travel + " << tour_res.total_visit_time_min << " visit)" << std::endl;

    // Verify all must-visits are in stops
    bool found_dg = false, found_ph = false;
    for (size_t i = 0; i < tour_res.stops.size(); ++i) {
        if (tour_res.stops[i].place_id == "DG") found_dg = true;
        if (tour_res.stops[i].place_id == "PH") found_ph = true;
    }
    assert(found_dg && found_ph);
    std::cout << "  -> PASSED" << std::endl;
}

static void test_regressions(ds::Graph& g) {
    planner::RouteQuery q; q.start_id = "SW"; q.end_id = "SB";
    const planner::Mode modes[] = {planner::Mode::Balanced, planner::Mode::Shortest,
        planner::Mode::Fastest, planner::Mode::Scenic, planner::Mode::LeastCrowded};
    // Exhaustive endpoint/mode comparison against the unpenalized objective solver.
    for (int u = 0; u < 10; ++u) for (int v = 0; v < 10; ++v) for (auto mode : modes) {
        q.primary_mode = mode;
        auto ranked = planner::Alternatives::generate_ranked_alternatives(g, u, v, q);
        planner::Dijkstra::Options opts; opts.mode = mode;
        auto expected = planner::Dijkstra::find_path(g, u, v, opts);
        assert(!ranked.empty() && expected.found);
        auto cost = [&](const planner::RouteResult& r) {
            double c = 0;
            for (const auto& leg : r.legs) c += planner::Scoring::compute_edge_cost(
                *g.get_road(leg.road_id), g.get_place(leg.to_idx), mode, q.weights, q.interests);
            return c;
        };
        assert(approx_equal(cost(ranked[0]), cost(expected)));
    }
    q.primary_mode = planner::Mode::Fastest;
    auto ranked = planner::Alternatives::generate_ranked_alternatives(g, g.get_place_index("SW"), g.get_place_index("SB"), q);
    assert(approx_equal(ranked[0].total_travel_time_min, 22.8));
    q.end_id = "RZ"; q.must_visit.push_back("DG"); q.primary_mode = planner::Mode::Scenic; q.max_time_min = 100;
    std::string err; auto tour = planner::Tour::plan_tour(g, q, err);
    assert(!tour.found || tour.total_time_min <= 100 + 1e-6);
    q.max_time_min = -1; q.max_distance_km = 1;
    assert(!planner::Tour::plan_tour(g, q, err).found);
    q.max_distance_km = -1; q.end_id = "SW"; q.max_time_min = 200;
    tour = planner::Tour::plan_tour(g, q, err);
    assert(tour.found && tour.stops[0].visit_minutes == 0 && tour.stops.back().visit_minutes == 0);
    double visits = 0; for (const auto& stop : tour.stops) visits += stop.visit_minutes;
    assert(approx_equal(visits, tour.total_visit_time_min));
    auto zero = planner::Dijkstra::find_path(g, 0, 0, planner::Dijkstra::Options{});
    assert(zero.stops[0].visit_minutes == 0 && zero.total_time_min == 0);
    for (const std::string invalid : {"{} trailing", "01", "1.", "1e+", "1e999", "[1,]", "{\"a\":1,\"a\":2}"}) {
        util::JsonValue::parse(invalid, &err); assert(!err.empty());
    }
    auto unicode = util::JsonValue::parse("\"\\u092a\\u0941\\u0923\\u0947\"", &err);
    assert(err.empty() && unicode.as_string() == "पुणे");
    auto escaped = util::JsonValue::object(); escaped["error"] = "bad \"id\"\n\\";
    auto decoded = util::JsonValue::parse(escaped.serialize(), &err);
    assert(err.empty() && decoded["error"].as_string() == escaped["error"].as_string());
    std::cout << "  Regression checks passed (500 mode/endpoint comparisons)" << std::endl;
}

int main(int argc, char** argv) {
    std::cout << "===========================================" << std::endl;
    std::cout << "   RUNNING DATA STRUCTURES TEST SUITE      " << std::endl;
    std::cout << "===========================================" << std::endl;

    std::string data_path = "tests/fixtures/pune_demo.json";
    if (argc > 1) {
        data_path = argv[1];
    }

    test_dyn_array();
    test_min_heap();
    test_hash_map();
    test_stack_queue();
    test_sort_stability();
    test_union_find_and_trie();

    ds::Graph g;
    bool loaded = load_demo_graph(data_path, g);
    if (!loaded) {
        // Try fallback path
        data_path = "../tests/fixtures/pune_demo.json";
        loaded = load_demo_graph(data_path, g);
    }
    if (!loaded) {
        data_path = "pune_route_planner/tests/fixtures/pune_demo.json";
        loaded = load_demo_graph(data_path, g);
    }
    assert(loaded && "Failed to load pune_demo.json dataset");
    assert(g.num_places() == 10);
    assert(g.num_roads() == 22);

    test_dijkstra_demo_benchmarks(g);
    test_dynamic_updates_undo_and_bfs(g);
    test_tour_planner(g);
    test_regressions(g);

    std::cout << "===========================================" << std::endl;
    std::cout << "   ALL TESTS PASSED WITH 100% SUCCESS!     " << std::endl;
    std::cout << "===========================================" << std::endl;
    return 0;
}

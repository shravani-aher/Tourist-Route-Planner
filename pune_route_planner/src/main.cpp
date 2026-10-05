#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>

#include "../third_party/httplib.h"

#include "ds/Graph.h"
#include "ds/Trie.h"
#include "ds/Sort.h"
#include "planner/Models.h"
#include "planner/Scoring.h"
#include "planner/Dijkstra.h"
#include "planner/Alternatives.h"
#include "planner/Tour.h"
#include "planner/Dynamic.h"
#include "util/Json.h"

static bool load_graph_from_json(const std::string& path, ds::Graph& graph, ds::Trie& trie) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open dataset at: " << path << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    std::string err;
    util::JsonValue root = util::JsonValue::parse(buffer.str(), &err);
    if (!err.empty()) {
        std::cerr << "JSON Parse error: " << err << std::endl;
        return false;
    }

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
        graph.add_place(p);

        // Populate Trie for place name and id autocomplete
        trie.insert(p.name, p.id, p.name);
        trie.insert(p.id, p.id, p.name);
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
        graph.add_road(r);
    }

    std::cout << "Loaded " << graph.num_places() << " places and "
              << graph.num_roads() << " roads from " << path << std::endl;
    return true;
}

static util::JsonValue serialize_route(const planner::RouteResult& r, const ds::Graph& graph) {
    util::JsonValue obj = util::JsonValue::object();
    obj["found"] = r.found;
    obj["message"] = r.message;
    obj["mode_label"] = r.mode_label;
    obj["total_distance_km"] = r.total_distance_km;
    obj["total_travel_time_min"] = r.total_travel_time_min;
    obj["total_visit_time_min"] = r.total_visit_time_min;
    obj["total_time_min"] = r.total_time_min;
    obj["avg_scenic"] = r.avg_scenic;
    obj["avg_crowd"] = r.avg_crowd;
    obj["balanced_cost"] = r.balanced_cost;
    obj["demo_index"] = r.demo_index;

    util::JsonValue nodes_arr = util::JsonValue::array();
    for (size_t i = 0; i < r.node_path.size(); ++i) {
        int idx = r.node_path[i];
        util::JsonValue n = util::JsonValue::object();
        n["index"] = idx;
        n["id"] = graph.get_place(idx).id;
        n["name"] = graph.get_place(idx).name;
        n["x"] = graph.get_place(idx).x;
        n["y"] = graph.get_place(idx).y;
        nodes_arr.push_back(n);
    }
    obj["nodes"] = nodes_arr;

    util::JsonValue roads_arr = util::JsonValue::array();
    for (size_t i = 0; i < r.road_path.size(); ++i) {
        roads_arr.push_back(r.road_path[i]);
    }
    obj["roads"] = roads_arr;

    util::JsonValue legs_arr = util::JsonValue::array();
    for (size_t i = 0; i < r.legs.size(); ++i) {
        const auto& leg = r.legs[i];
        util::JsonValue l = util::JsonValue::object();
        l["road_id"] = leg.road_id;
        l["from_id"] = leg.from_id;
        l["to_id"] = leg.to_id;
        l["distance_km"] = leg.distance_km;
        l["travel_time_min"] = leg.travel_time_min;
        l["scenic"] = leg.scenic;
        l["crowd"] = leg.crowd;
        l["traffic"] = leg.traffic;
        legs_arr.push_back(l);
    }
    obj["legs"] = legs_arr;

    util::JsonValue stops_arr = util::JsonValue::array();
    for (size_t i = 0; i < r.stops.size(); ++i) {
        const auto& st = r.stops[i];
        util::JsonValue s = util::JsonValue::object();
        s["place_id"] = st.place_id;
        s["name"] = st.name;
        s["visit_minutes"] = st.visit_minutes;
        s["is_must_visit"] = st.is_must_visit;
        s["is_start"] = st.is_start;
        s["is_end"] = st.is_end;
        stops_arr.push_back(s);
    }
    obj["stops"] = stops_arr;

    util::JsonValue settled_arr = util::JsonValue::array();
    for (size_t i = 0; i < r.settled_order.size(); ++i) {
        settled_arr.push_back(r.settled_order[i]);
    }
    obj["settled_order"] = settled_arr;

    return obj;
}

static util::JsonValue serialize_diff(const planner::RouteDiff& diff) {
    util::JsonValue d = util::JsonValue::object();
    d["has_old"] = diff.has_old;
    d["has_new"] = diff.has_new;
    d["delta_distance_km"] = diff.delta_distance_km;
    d["delta_time_min"] = diff.delta_time_min;
    d["delta_balanced_cost"] = diff.delta_balanced_cost;
    d["summary"] = diff.summary;

    util::JsonValue added = util::JsonValue::array();
    for (size_t i = 0; i < diff.roads_added.size(); ++i) added.push_back(diff.roads_added[i]);
    d["roads_added"] = added;

    util::JsonValue removed = util::JsonValue::array();
    for (size_t i = 0; i < diff.roads_removed.size(); ++i) removed.push_back(diff.roads_removed[i]);
    d["roads_removed"] = removed;

    return d;
}

static util::JsonValue serialize_graph(const ds::Graph& graph, const planner::DynamicManager& dm) {
    util::JsonValue root = util::JsonValue::object();
    root["city"] = "Pune";
    root["warning"] = "Real attraction names; all numeric attributes and road corridors are synthetic classroom examples, not verified navigation or live conditions.";
    root["graph_version"] = static_cast<int64_t>(graph.version());

    ds::DynArray<int> comp_ids;
    int comp_count = dm.compute_connected_components(&comp_ids);
    root["connected_components"] = comp_count;

    util::JsonValue places_arr = util::JsonValue::array();
    for (size_t i = 0; i < graph.num_places(); ++i) {
        const auto& p = graph.get_place(static_cast<int>(i));
        util::JsonValue pv = util::JsonValue::object();
        pv["id"] = p.id;
        pv["name"] = p.name;
        pv["visit_minutes"] = p.visit_minutes;
        pv["crowd"] = p.crowd;
        pv["x"] = p.x;
        pv["y"] = p.y;
        pv["index"] = p.index;
        pv["component_id"] = (i < comp_ids.size()) ? comp_ids[i] : 0;

        util::JsonValue cats = util::JsonValue::array();
        for (const auto& c : p.categories) cats.push_back(c);
        pv["categories"] = cats;

        places_arr.push_back(pv);
    }
    root["places"] = places_arr;

    util::JsonValue roads_arr = util::JsonValue::array();
    const auto& r_ids = graph.all_road_ids();
    for (size_t i = 0; i < r_ids.size(); ++i) {
        const auto* r = graph.get_road(r_ids[i]);
        if (!r) continue;
        util::JsonValue rv = util::JsonValue::object();
        rv["id"] = r->id;
        rv["u"] = r->u;
        rv["v"] = r->v;
        rv["u_idx"] = r->u_idx;
        rv["v_idx"] = r->v_idx;
        rv["directed"] = r->directed;
        rv["distance_km"] = r->distance_km;
        rv["base_time_min"] = r->base_time_min;
        rv["effective_time_min"] = r->effective_time_min();
        rv["scenic"] = r->scenic;
        rv["crowd"] = r->crowd;
        rv["traffic"] = r->traffic;
        rv["blocked"] = r->blocked;
        roads_arr.push_back(rv);
    }
    root["roads"] = roads_arr;

    return root;
}

int main(int argc, char** argv) {
    std::string data_path = "data/pune_demo.json";
    std::string web_dir = "web";
    int port = 8080;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--data" && i + 1 < argc) data_path = argv[++i];
        else if (arg == "--web" && i + 1 < argc) web_dir = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
    }

    ds::Graph graph;
    ds::Trie trie;
    if (!load_graph_from_json(data_path, graph, trie)) {
        // Fallback search paths
        if (load_graph_from_json("../data/pune_demo.json", graph, trie)) {
            data_path = "../data/pune_demo.json";
            web_dir = "../web";
        } else if (load_graph_from_json("pune_route_planner/data/pune_demo.json", graph, trie)) {
            data_path = "pune_route_planner/data/pune_demo.json";
            web_dir = "pune_route_planner/web";
        } else {
            std::cerr << "Fatal error: Could not find pune_demo.json!" << std::endl;
            return 1;
        }
    }

    planner::DynamicManager dynamic_mgr(graph);

    httplib::Server svr;

    // CORS headers for all responses
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });

    svr.Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
    });

    // 1. GET /api/graph
    svr.Get("/api/graph", [&](const httplib::Request&, httplib::Response& res) {
        util::JsonValue g_json = serialize_graph(graph, dynamic_mgr);
        res.set_content(g_json.serialize(), "application/json");
    });

    // 2. GET /api/autocomplete?q=...
    svr.Get("/api/autocomplete", [&](const httplib::Request& req, httplib::Response& res) {
        std::string q = req.has_param("q") ? req.get_param_value("q") : "";
        auto matches = trie.search_prefix(q, 10);
        util::JsonValue arr = util::JsonValue::array();
        for (size_t i = 0; i < matches.size(); ++i) {
            util::JsonValue item = util::JsonValue::object();
            item["place_id"] = matches[i].place_id;
            item["full_name"] = matches[i].full_name;
            arr.push_back(item);
        }
        res.set_content(arr.serialize(), "application/json");
    });

    // 3. POST /api/route
    svr.Post("/api/route", [&](const httplib::Request& req, httplib::Response& res) {
        std::string err;
        util::JsonValue body = util::JsonValue::parse(req.body, &err);
        if (!err.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON: " + err + "\"}", "application/json");
            return;
        }

        std::string start_id = body["start"].as_string();
        std::string end_id = body["end"].as_string();

        if (start_id.empty() || end_id.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"Missing start or end place ID\"}", "application/json");
            return;
        }

        int start_idx = graph.get_place_index(start_id);
        int end_idx = graph.get_place_index(end_id);

        if (start_idx < 0 || end_idx < 0) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid start or end place ID not found in Pune network\"}", "application/json");
            return;
        }

        planner::RouteQuery query;
        query.start_id = start_id;
        query.end_id = end_id;
        query.primary_mode = planner::string_to_mode(body["mode"].as_string_or("balanced"));
        query.max_time_min = body["maxTimeMin"].as_double(-1.0);
        query.max_distance_km = body["maxDistanceKm"].as_double(-1.0);
        query.k_alternatives = body["k"].as_int(5);

        // Parse weights if provided
        if (body.has_key("weights")) {
            const auto& w_obj = body["weights"];
            query.weights.wd = w_obj["wd"].as_double(0.20);
            query.weights.wt = w_obj["wt"].as_double(0.30);
            query.weights.ws = w_obj["ws"].as_double(0.20);
            query.weights.wc = w_obj["wc"].as_double(0.15);
            query.weights.wp = w_obj["wp"].as_double(0.15);
        }

        std::string weight_err;
        if (!query.weights.validate_and_normalize(weight_err)) {
            res.status = 400;
            res.set_content("{\"error\":\"" + weight_err + "\"}", "application/json");
            return;
        }

        // Parse interests
        if (body.has_key("interests")) {
            for (const auto& item : body["interests"].as_array()) {
                query.interests.push_back(item.as_string());
            }
        }

        // Parse avoid categories
        if (body.has_key("avoid")) {
            for (const auto& item : body["avoid"].as_array()) {
                query.avoid.push_back(item.as_string());
            }
        }

        // Parse must_visit places
        if (body.has_key("mustVisit")) {
            for (const auto& item : body["mustVisit"].as_array()) {
                query.must_visit.push_back(item.as_string());
            }
        }

        util::JsonValue resp = util::JsonValue::object();
        resp["city"] = "Pune";
        resp["warning"] = "Real attraction names; all numeric attributes and road corridors are synthetic classroom examples, not verified navigation or live conditions.";

        if (!query.must_visit.empty()) {
            // Personalized Tour mode
            std::string tour_err;
            planner::RouteResult tour_res = planner::Tour::plan_tour(graph, query, tour_err);
            if (!tour_res.found) {
                res.status = 400;
                res.set_content("{\"error\":\"" + tour_err + "\"}", "application/json");
                return;
            }

            dynamic_mgr.set_active_query(query, tour_res);
            resp["is_tour"] = true;
            resp["best_route"] = serialize_route(tour_res, graph);

            util::JsonValue ranked_arr = util::JsonValue::array();
            ranked_arr.push_back(serialize_route(tour_res, graph));
            resp["ranked_routes"] = ranked_arr;
        } else {
            // Point-to-point mode with full 5-mode comparison and K-alternatives
            auto ranked = planner::Alternatives::generate_ranked_alternatives(
                graph, start_idx, end_idx, query
            );

            if (ranked.empty()) {
                res.status = 400;
                res.set_content("{\"error\":\"No feasible route found in this candidate pool.\"}", "application/json");
                return;
            }

            dynamic_mgr.set_active_query(query, ranked[0]);

            resp["is_tour"] = false;
            resp["best_route"] = serialize_route(ranked[0], graph);

            util::JsonValue ranked_arr = util::JsonValue::array();
            for (size_t i = 0; i < ranked.size(); ++i) {
                ranked_arr.push_back(serialize_route(ranked[i], graph));
            }
            resp["ranked_routes"] = ranked_arr;

            // Compute standard mode comparison cards
            util::JsonValue modes_obj = util::JsonValue::object();
            const planner::Mode comparison_modes[] = {
                planner::Mode::Balanced,
                planner::Mode::Shortest,
                planner::Mode::Fastest,
                planner::Mode::Scenic,
                planner::Mode::LeastCrowded
            };
            for (auto m : comparison_modes) {
                planner::Dijkstra::Options m_opts;
                m_opts.mode = m;
                m_opts.weights = query.weights;
                m_opts.interests = query.interests;
                auto m_res = planner::Dijkstra::find_path(graph, start_idx, end_idx, m_opts);
                modes_obj[planner::mode_to_string(m)] = serialize_route(m_res, graph);
            }
            resp["mode_routes"] = modes_obj;
        }

        res.set_content(resp.serialize(), "application/json");
    });

    // 4. POST /api/update
    svr.Post("/api/update", [&](const httplib::Request& req, httplib::Response& res) {
        std::string err;
        util::JsonValue body = util::JsonValue::parse(req.body, &err);
        if (!err.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON: " + err + "\"}", "application/json");
            return;
        }

        std::string type_str = body["type"].as_string();
        std::string target_id = body.has_key("roadId") ? body["roadId"].as_string() : body["placeId"].as_string();

        if (target_id.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"Missing roadId or placeId\"}", "application/json");
            return;
        }

        planner::UpdateType type = planner::UpdateType::BlockRoad;
        double num_val = 0.0;
        bool bool_val = false;

        if (type_str == "block") {
            type = planner::UpdateType::BlockRoad;
            bool_val = true;
        } else if (type_str == "unblock") {
            type = planner::UpdateType::UnblockRoad;
            bool_val = false;
        } else if (type_str == "traffic") {
            type = planner::UpdateType::ChangeTraffic;
            num_val = body["value"].as_double(5.0);
        } else if (type_str == "road_crowd") {
            type = planner::UpdateType::ChangeRoadCrowd;
            num_val = body["value"].as_double(5.0);
        } else if (type_str == "place_crowd") {
            type = planner::UpdateType::ChangePlaceCrowd;
            num_val = body["value"].as_double(5.0);
        } else {
            res.status = 400;
            res.set_content("{\"error\":\"Unknown update type: " + type_str + "\"}", "application/json");
            return;
        }

        planner::RouteDiff diff;
        std::string op_err;
        bool success = dynamic_mgr.apply_update(type, target_id, num_val, bool_val, diff, op_err);
        if (!success) {
            res.status = 400;
            res.set_content("{\"error\":\"" + op_err + "\"}", "application/json");
            return;
        }

        util::JsonValue resp = util::JsonValue::object();
        resp["success"] = true;
        resp["diff"] = serialize_diff(diff);
        if (dynamic_mgr.has_active_route()) {
            resp["current_route"] = serialize_route(dynamic_mgr.current_route(), graph);
        }
        resp["graph"] = serialize_graph(graph, dynamic_mgr);

        res.set_content(resp.serialize(), "application/json");
    });

    // 5. POST /api/undo
    svr.Post("/api/undo", [&](const httplib::Request&, httplib::Response& res) {
        planner::RouteDiff diff;
        std::string err;
        bool ok = dynamic_mgr.undo(diff, err);
        if (!ok) {
            res.status = 400;
            res.set_content("{\"error\":\"" + err + "\"}", "application/json");
            return;
        }

        util::JsonValue resp = util::JsonValue::object();
        resp["success"] = true;
        resp["diff"] = serialize_diff(diff);
        if (dynamic_mgr.has_active_route()) {
            resp["current_route"] = serialize_route(dynamic_mgr.current_route(), graph);
        }
        resp["graph"] = serialize_graph(graph, dynamic_mgr);

        res.set_content(resp.serialize(), "application/json");
    });

    // 6. POST /api/simulate
    svr.Post("/api/simulate", [&](const httplib::Request&, httplib::Response& res) {
        std::string event_title;
        planner::RouteDiff diff;
        std::string err;
        bool ok = dynamic_mgr.trigger_next_simulated_event(event_title, diff, err);
        if (!ok) {
            res.status = 400;
            res.set_content("{\"error\":\"" + err + "\"}", "application/json");
            return;
        }

        util::JsonValue resp = util::JsonValue::object();
        resp["success"] = true;
        resp["event_title"] = event_title;
        resp["diff"] = serialize_diff(diff);
        if (dynamic_mgr.has_active_route()) {
            resp["current_route"] = serialize_route(dynamic_mgr.current_route(), graph);
        }
        resp["graph"] = serialize_graph(graph, dynamic_mgr);

        res.set_content(resp.serialize(), "application/json");
    });

    // 7. GET /api/stats
    svr.Get("/api/stats", [&](const httplib::Request&, httplib::Response& res) {
        util::JsonValue stats = util::JsonValue::object();
        stats["num_places"] = static_cast<int64_t>(graph.num_places());
        stats["num_roads"] = static_cast<int64_t>(graph.num_roads());
        stats["hash_load_factor"] = graph.road_table().load_factor();
        stats["hash_bucket_count"] = static_cast<int64_t>(graph.road_table().bucket_count());
        stats["hash_elements_count"] = static_cast<int64_t>(graph.road_table().size());
        stats["undo_stack_depth"] = static_cast<int64_t>(dynamic_mgr.undo_stack_depth());
        stats["pending_events_count"] = static_cast<int64_t>(dynamic_mgr.pending_events_count());
        stats["connected_components"] = dynamic_mgr.compute_connected_components();
        stats["graph_version"] = static_cast<int64_t>(graph.version());

        res.set_content(stats.serialize(), "application/json");
    });

    // Mount static web files
    svr.set_mount_point("/", web_dir.c_str());

    std::cout << "==========================================================" << std::endl;
    std::cout << "  Pune Tourist Route Planner Server running on http://127.0.0.1:" << port << std::endl;
    std::cout << "  Open your browser to: http://127.0.0.1:" << port << std::endl;
    std::cout << "==========================================================" << std::endl;

    if (!svr.listen("127.0.0.1", port)) {
        std::cerr << "Failed to bind to 127.0.0.1:" << port << std::endl;
        return 1;
    }

    return 0;
}

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <cmath>
#include <mutex>

#include "../third_party/httplib.h"

#include "ds/Graph.h"
#include "ds/Trie.h"
#include "ds/Sort.h"
#include "planner/Models.h"
#include "planner/Dataset.h"
static util::JsonValue dataset_metadata;
static bool test_fixture=false;
#include "planner/Scoring.h"
#include "planner/Dijkstra.h"
#include "planner/Alternatives.h"
#include "planner/Tour.h"
#include "planner/Solve.h"
#include "planner/Dynamic.h"
#include "planner/TimeContext.h"
#include "util/Json.h"

static void json_error(httplib::Response& res, const std::string& message) {
    res.status = 400;
    auto body = util::JsonValue::object(); body["error"] = message;
    res.set_content(body.serialize(), "application/json");
}
static std::string validate_request(const util::JsonValue& body, bool route) {
    if (!body.is_object()) return "Request must be a JSON object";
    if (route) {
        for (const auto& key : {"start", "end"})
            if (!body[key].is_string() || body[key].as_string().empty()) return std::string(key) + " must be a nonempty string";
        if (body.has_key("mode")) {
            const auto& v = body["mode"];
            if (!v.is_string() || (v.as_string() != "balanced" && v.as_string() != "shortest" &&
                v.as_string() != "fastest" && v.as_string() != "scenic" && v.as_string() != "least_crowded")) return "Unknown route mode";
        }
        for (const auto& key : {"maxTimeMin", "maxDistanceKm", "maxDetourRatio", "k"}) {
            if (!body.has_key(key)) continue;
            const auto& v = body[key];
            if (!v.is_number() || !std::isfinite(v.as_double())) return std::string(key) + " must be a finite number";
            double n = v.as_double();
            if (std::string(key) == "k") {
                if (n < 1 || n > 20 || std::floor(n) != n) return "k must be an integer from 1 to 20";
            } else if (std::string(key) == "maxDetourRatio") {
                if (n != -1 && n < 1) return "maxDetourRatio must be -1 or at least 1";
            } else if (n != -1 && n <= 0) return std::string(key) + " must be positive or -1 (unlimited)";
        }
        for (const auto& key : {"interests", "avoid", "mustVisit"}) {
            if (!body.has_key(key)) continue;
            if (!body[key].is_array()) return std::string(key) + " must be an array of strings";
            for (const auto& item : body[key].as_array())
                if (!item.is_string() || item.as_string().empty()) return std::string(key) + " must contain nonempty strings";
        }
        if (body.has_key("weights")) {
            if (!body["weights"].is_object()) return "weights must be an object";
            for (const auto& key : {"wd", "wt", "ws", "wc", "wp"}) {
                const auto& w = body["weights"];
                if (w.has_key(key) && (!w[key].is_number() || !std::isfinite(w[key].as_double()) || w[key].as_double() < 0 || w[key].as_double() > 1))
                    return "weights must be finite numbers from 0 to 1";
            }
        }
    } else {
        if (!body["type"].is_string()) return "type must be a string";
        const std::string t = body["type"].as_string();
        if (t != "block" && t != "unblock" && t != "traffic" && t != "road_crowd" && t != "place_crowd") return "Unknown update type";
        const char* key = t == "place_crowd" ? "placeId" : "roadId";
        if (!body[key].is_string() || body[key].as_string().empty()) return std::string(key) + " must be a nonempty string";
        if (t != "block" && t != "unblock") {
            const auto& v = body["value"];
            if (!v.is_number() || !std::isfinite(v.as_double()) || v.as_double() < 0 || v.as_double() > 10) return "value must be a finite number from 0 to 10";
            if (t == "place_crowd" && std::floor(v.as_double()) != v.as_double()) return "place crowd must be an integer";
        }
    }
    return "";
}

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
    obj["total_visit_time_min"] = (graph.real_data && !graph.metrics_estimated) ? util::JsonValue::null() : util::JsonValue(r.total_visit_time_min);
    obj["total_time_min"] = r.total_time_min;
    obj["avg_scenic"] = (graph.real_data && !graph.metrics_estimated) ? util::JsonValue::null() : util::JsonValue(r.avg_scenic);
    obj["avg_crowd"] = (graph.real_data && !graph.metrics_estimated) ? util::JsonValue::null() : util::JsonValue(r.avg_crowd);
    obj["balanced_cost"] = r.balanced_cost;
    obj["demo_index"] = (graph.real_data && !graph.metrics_estimated) ? util::JsonValue::null() : util::JsonValue(r.demo_index);

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
        l["scenic"] = (graph.real_data && !graph.metrics_estimated) ? util::JsonValue::null() : util::JsonValue(leg.scenic);
        l["crowd"] = (graph.real_data && !graph.metrics_estimated) ? util::JsonValue::null() : util::JsonValue(leg.crowd);
        l["traffic"] = graph.real_data ? util::JsonValue::null() : util::JsonValue(leg.traffic);
        legs_arr.push_back(l);
    }
    obj["legs"] = legs_arr;

    util::JsonValue stops_arr = util::JsonValue::array();
    for (size_t i = 0; i < r.stops.size(); ++i) {
        const auto& st = r.stops[i];
        util::JsonValue s = util::JsonValue::object();
        s["place_id"] = st.place_id;
        s["name"] = st.name;
        s["visit_minutes"] = (graph.real_data && !graph.metrics_estimated) ? util::JsonValue::null() : util::JsonValue(st.visit_minutes);
        if (st.arrive_min >= 0) { s["arrive_min"] = st.arrive_min; s["depart_min"] = st.depart_min; s["wait_min"] = st.wait_min; }
        if (st.crowd_at_arrival >= 0) s["crowd_at_arrival"] = st.crowd_at_arrival;
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
    root["real_data"] = graph.real_data;
    if (graph.real_data) { root["manifest"]=dataset_metadata["manifest"]; root["attractions"]=dataset_metadata["attractions"]; }
    root["warning"] = graph.real_data ? "Offline OSM driving graph. Estimated free-flow time; live traffic and live crowd unavailable. Scenic scores, crowd curves, visit durations and hours are model/curated estimates. Approach snaps and access are not field-verified. Not turn-by-turn navigation." : "Real attraction names; all numeric attributes and road corridors are synthetic classroom examples, not verified navigation or live conditions.";
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
        bool has_meta = !graph.real_data || p.metrics_estimated;
        pv["visit_minutes"] = has_meta ? util::JsonValue(p.visit_minutes) : util::JsonValue::null();
        pv["crowd"] = has_meta ? util::JsonValue(p.crowd) : util::JsonValue::null();
        pv["metrics_estimated"] = p.metrics_estimated;
        pv["attraction"] = p.attraction;
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
        rv["scenic"] = (graph.real_data && r->highway.empty()) ? util::JsonValue::null() : util::JsonValue(r->scenic);
        rv["highway"] = r->highway;
        rv["crowd"] = (graph.real_data && !graph.metrics_estimated) ? util::JsonValue::null() : util::JsonValue(r->crowd);
        rv["traffic"] = graph.real_data ? util::JsonValue::null() : util::JsonValue(r->traffic);
        rv["osm_way"] = r->osm_way;
        rv["name"] = r->name;
        rv["blocked"] = r->blocked;
        roads_arr.push_back(rv);
    }
    root["roads"] = roads_arr;

    return root;
}

int main(int argc, char** argv) {
    std::string data_path = "data/pune_driving.json";
    std::string web_dir = "web_real";
    int port = 8080;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test-fixture") test_fixture = true;
        else if (arg == "--data" && i + 1 < argc) data_path = argv[++i];
        else if (arg == "--web" && i + 1 < argc) web_dir = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
    }

    ds::Graph graph;
    ds::Trie trie;
    std::string load_error;
    bool loaded = planner::load_dataset(data_path, graph, trie, dataset_metadata, test_fixture, load_error);
    if (!loaded && test_fixture && load_error.empty()) loaded=load_graph_from_json(data_path,graph,trie);
    if (!loaded) { std::cerr << "Dataset rejected: " << load_error << std::endl; return 1; }

    planner::DynamicManager dynamic_mgr(graph);

    httplib::Server svr;
    std::mutex state_mutex;
    svr.set_payload_max_length(64 * 1024);
    // Bounded thread count avoids one 8 MiB stack per reported CPU on large hosts.
    svr.new_task_queue = [] { return new httplib::ThreadPool(4); };
    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.get_header_value("Content-Type").find("application/json") == 0) return;
        auto error = util::JsonValue::object();
        error["error"] = res.status == 413 ? "Request body exceeds 64 KiB" : "HTTP request rejected";
        res.set_content(error.serialize(), "application/json");
    });

    // CORS headers for all responses
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });

    svr.Options(R"(.*)", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(state_mutex);
        res.status = 200;
    });

    // 1. GET /api/graph
    svr.Get("/api/graph", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(state_mutex);
        util::JsonValue g_json = serialize_graph(graph, dynamic_mgr);
        res.set_content(g_json.serialize(), "application/json");
    });

    // 2. GET /api/autocomplete?q=...
    svr.Get("/api/autocomplete", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(state_mutex);
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
        std::lock_guard<std::mutex> lock(state_mutex);
        std::string err;
        util::JsonValue body = util::JsonValue::parse(req.body, &err);
        if (!err.empty()) {
            json_error(res, "Invalid JSON: " + err);
            return;
        }

        auto schema_error = validate_request(body, true);
        if (!schema_error.empty()) { json_error(res, schema_error); return; }
        std::string start_id = body["start"].as_string();
        std::string end_id = body["end"].as_string();

        if (start_id.empty() || end_id.empty()) {
            json_error(res, "Missing start or end place ID");
            return;
        }

        int start_idx = graph.get_place_index(start_id);
        int end_idx = graph.get_place_index(end_id);

        if (start_idx < 0 || end_idx < 0) {
            json_error(res, "Invalid start or end place ID not found in Pune network");
            return;
        }

        planner::RouteQuery query;
        query.start_id = start_id;
        query.end_id = end_id;
        query.primary_mode = planner::string_to_mode(body["mode"].as_string_or("balanced"));
        if (body.has_key("hour")) {
            double h = body["hour"].as_double(-1);
            if (!body["hour"].is_number() || h < 0 || h > 23 || std::floor(h) != h) { json_error(res, "hour must be an integer 0-23"); return; }
            planner::time_context().hour = (int)h;
        } else planner::time_context().hour = 12;
        if (body.has_key("weekend")) {
            if (!body["weekend"].is_bool()) { json_error(res, "weekend must be true or false"); return; }
            planner::time_context().weekend = body["weekend"].as_bool();
        } else planner::time_context().weekend = false;
        if (graph.real_data) {
            if (!graph.metrics_estimated && query.primary_mode != planner::Mode::Shortest && query.primary_mode != planner::Mode::Fastest) {
                json_error(res, "Only shortest and estimated-fastest are available without verified condition metrics"); return;
            }
            if (!graph.metrics_estimated && (body["mustVisit"].size() || body["interests"].size() || body["avoid"].size())) {
                json_error(res, "Tours and interest scoring unavailable until verified visit durations/catalog metadata are supplied"); return;
            }
        }
        query.max_time_min = body["maxTimeMin"].as_double(-1.0);
        query.max_distance_km = body["maxDistanceKm"].as_double(-1.0);
        query.k_alternatives = body["k"].as_int(graph.real_data ? 1 : 5);
        query.max_detour_ratio = body["maxDetourRatio"].as_double(-1.0);
        if(graph.real_data && !graph.metrics_estimated && query.k_alternatives!=1) { json_error(res,"Real-data release currently supports one exact unconstrained objective route; set k=1"); return; }
        if(graph.real_data && query.k_alternatives>5) { json_error(res,"k must be 1-5 on the real graph"); return; }

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
        if (graph.real_data && !graph.metrics_estimated) { query.weights.wd=.5;query.weights.wt=.5;query.weights.ws=0;query.weights.wc=0;query.weights.wp=0; }
        if (!query.weights.validate_and_normalize(weight_err)) {
            json_error(res, weight_err);
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
        resp["warning"] = graph.real_data ? "Offline OSM driving graph. Estimated free-flow time; live traffic and live crowd unavailable. Scenic scores, crowd curves, visit durations and hours are model/curated estimates. Approach snaps and access are not field-verified. Not turn-by-turn navigation." : "Real attraction names; all numeric attributes and road corridors are synthetic classroom examples, not verified navigation or live conditions.";

        if (!query.must_visit.empty()) {
            // Personalized Tour mode
            std::string tour_err;
            planner::RouteResult tour_res = planner::solve(graph, query).best;
            if (!tour_res.found) {
                json_error(res, tour_res.message);
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
            auto ranked = planner::solve(graph, query).ranked;

            if (ranked.empty()) {
                json_error(res, "No feasible route found in this candidate pool.");
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
                if (graph.real_data && !graph.metrics_estimated && m != planner::Mode::Shortest && m != planner::Mode::Fastest) continue;
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
        if (graph.real_data) { json_error(res,"Manual/simulated condition mutations disabled for production data"); return; }
        std::lock_guard<std::mutex> lock(state_mutex);
        std::string err;
        util::JsonValue body = util::JsonValue::parse(req.body, &err);
        if (!err.empty()) {
            json_error(res, "Invalid JSON: " + err);
            return;
        }

        auto schema_error = validate_request(body, false);
        if (!schema_error.empty()) { json_error(res, schema_error); return; }
        std::string type_str = body["type"].as_string();
        std::string target_id = type_str == "place_crowd" ? body["placeId"].as_string() : body["roadId"].as_string();

        if (target_id.empty()) {
            json_error(res, "Missing roadId or placeId");
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
            json_error(res, "Unknown update type: " + type_str + "");
            return;
        }

        planner::RouteDiff diff;
        std::string op_err;
        bool success = dynamic_mgr.apply_update(type, target_id, num_val, bool_val, diff, op_err);
        if (!success) {
            json_error(res, op_err);
            return;
        }

        util::JsonValue resp = util::JsonValue::object();
        resp["success"] = true;
        resp["diff"] = serialize_diff(diff);
        if (dynamic_mgr.has_active_query()) {
            resp["current_route"] = serialize_route(dynamic_mgr.current_route(), graph);
        }
        resp["graph"] = serialize_graph(graph, dynamic_mgr);

        res.set_content(resp.serialize(), "application/json");
    });

    // 5. POST /api/undo
    svr.Post("/api/undo", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(state_mutex);
        planner::RouteDiff diff;
        std::string err;
        bool ok = dynamic_mgr.undo(diff, err);
        if (!ok) {
            json_error(res, err);
            return;
        }

        util::JsonValue resp = util::JsonValue::object();
        resp["success"] = true;
        resp["diff"] = serialize_diff(diff);
        if (dynamic_mgr.has_active_query()) {
            resp["current_route"] = serialize_route(dynamic_mgr.current_route(), graph);
        }
        resp["graph"] = serialize_graph(graph, dynamic_mgr);

        res.set_content(resp.serialize(), "application/json");
    });

    // 6. POST /api/simulate
    svr.Post("/api/simulate", [&](const httplib::Request&, httplib::Response& res) {
        if (graph.real_data) { json_error(res,"Simulation disabled for production data"); return; }
        std::lock_guard<std::mutex> lock(state_mutex);
        std::string event_title;
        planner::RouteDiff diff;
        std::string err;
        bool ok = dynamic_mgr.trigger_next_simulated_event(event_title, diff, err);
        if (!ok) {
            json_error(res, err);
            return;
        }

        util::JsonValue resp = util::JsonValue::object();
        resp["success"] = true;
        resp["event_title"] = event_title;
        resp["diff"] = serialize_diff(diff);
        if (dynamic_mgr.has_active_query()) {
            resp["current_route"] = serialize_route(dynamic_mgr.current_route(), graph);
        }
        resp["graph"] = serialize_graph(graph, dynamic_mgr);

        res.set_content(resp.serialize(), "application/json");
    });

    // 7. GET /api/stats
    svr.Get("/api/stats", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(state_mutex);
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

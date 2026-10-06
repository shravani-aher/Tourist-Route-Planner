#pragma once
#include "../ds/Graph.h"
#include "../ds/Trie.h"
#include "../util/Json.h"
#include <fstream>
#include <sstream>
#include <set>
#include <cmath>
namespace planner {
inline bool load_dataset(const std::string& path, ds::Graph& graph, ds::Trie& trie,
                         util::JsonValue& metadata, bool allow_synthetic, std::string& error) {
 try {
    std::ifstream f(path); if (!f) throw std::runtime_error("Cannot open dataset: " + path);
    std::stringstream b; b << f.rdbuf(); auto root=util::JsonValue::parse(b.str(), &error);
    if (!error.empty()) throw std::runtime_error(error);
    if (!root.is_object()) throw std::runtime_error("Dataset must be an object");
    const int schema=root["schema_version"].as_int();
    if (schema!=2 && schema!=3) {
        if (!allow_synthetic) throw std::runtime_error("Production requires schema v2/v3 OSM data; synthetic fixtures require --test-fixture");
        return false; // legacy fixture is handled only by explicit test mode
    }
    for (const auto& key : {"places", "mock", "synthetic"}) if (root.has_key(key)) throw std::runtime_error("Synthetic/legacy fields forbidden in production schema");
    const auto& m=root["manifest"];
    if (m["kind"].as_string()!="osm_driving" || m["profile"].as_string()!="motorcar" ||
        m["license"].as_string()!="ODbL-1.0" || m["source_sha256"].as_string().size()!=64 || m["source_sha256"].as_string().find_first_not_of("0123456789abcdef")!=std::string::npos ||
        m["source_url"].as_string().empty() || m["snapshot"].as_string().empty() || m["attribution"].as_string().empty())
        throw std::runtime_error("Invalid provenance manifest");
    if (!m["bbox"].is_array() || m["bbox"].size()!=4 || m["bbox"][0].as_double()!=18.485 || m["bbox"][1].as_double()!=73.825 || m["bbox"][2].as_double()!=18.565 || m["bbox"][3].as_double()!=73.915 || m["time_model"].as_string().empty()) throw std::runtime_error("Unsupported bounds or missing time provenance");
    if (!root["nodes"].is_array() || root["nodes"].size()==0 || !root["roads"].is_array() || root["roads"].size()==0 ||
        !root["attractions"].is_array() || root["attractions"].size()==0 || !root["turn_restrictions"].is_array()) throw std::runtime_error("Missing graph/catalog arrays");
    ds::Graph staged; staged.real_data=true; ds::Trie staged_trie;
    for (const auto& n:root["nodes"].as_array()) {
        ds::Place p; p.id=n["id"].as_string();p.name=p.id;p.attraction=false;
        if(p.id.empty() || staged.has_place(p.id) || !n["lon"].is_number() || !n["lat"].is_number()) throw std::runtime_error("Invalid/duplicate node");
        p.x=n["lon"].as_double();p.y=n["lat"].as_double();
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x<73.825||p.x>73.915||p.y<18.485||p.y>18.565) throw std::runtime_error("Coordinate outside bounded Pune area");
        staged.add_place(p);
    }
    for(const auto& n:root["roads"].as_array()) {
        for(const auto& key:{"distance_km_demo","base_time_min_demo","crowd_demo","traffic_demo","scenic_demo"}) if(n.has_key(key)) throw std::runtime_error("Synthetic road attributes forbidden");
        ds::Road r; r.id=n["id"].as_string();r.u=n["u"].as_string();r.v=n["v"].as_string();r.osm_way=n["osm_way"].as_string();r.name=n["name"].as_string();
        r.directed=n["directed"].as_bool();
        if(schema==3){ if(!n["scenic"].is_number()||!std::isfinite(n["scenic"].as_double())||n["scenic"].as_double()<0||n["scenic"].as_double()>10||!n["highway"].is_string()||n["highway"].as_string().empty()) throw std::runtime_error("Schema v3 road needs scenic 0-10 and highway"); r.scenic=n["scenic"].as_double(); r.highway=n["highway"].as_string(); }r.distance_km=n["distance_km"].as_double(-1);r.base_time_min=n["base_time_min"].as_double(-1);
        if(!n["distance_km"].is_number() || !n["base_time_min"].is_number() || !n["speed_kph"].is_number() || n["speed_kph"].as_double()<=0 || n["speed_kph"].as_double()>130 || n["time_basis"].as_string().empty() || std::abs(r.base_time_min-r.distance_km/n["speed_kph"].as_double()*60)>1e-6 || r.u==r.v || r.id.empty()||r.osm_way.empty()||staged.get_road(r.id)||!n["directed"].is_bool()||!std::isfinite(r.distance_km)||!std::isfinite(r.base_time_min)||r.distance_km<=0||r.distance_km>20||r.base_time_min<=0||r.base_time_min>120||!staged.add_road(r)) throw std::runtime_error("Invalid/duplicate road or reference");
    }
    // Distances must agree with the actual segment geometry, not an arbitrary
    // claimed numeric field. Times must agree with the declared speed model.
    for(const auto& id:staged.all_road_ids()) {
        const auto* r=staged.get_road(id);const auto& a=staged.get_place(r->u_idx);const auto& z=staged.get_place(r->v_idx);
        constexpr double radians=3.14159265358979323846/180;
        double lat1=a.y*radians,lat2=z.y*radians,dl=(z.x-a.x)*radians,dt=lat2-lat1;
        double h=std::sin(dt/2)*std::sin(dt/2)+std::cos(lat1)*std::cos(lat2)*std::sin(dl/2)*std::sin(dl/2);
        double distance=6371.0088*2*std::asin(std::sqrt(std::min(1.0,h)));
        if(std::abs(distance-r->distance_km)>1e-6) throw std::runtime_error("Distance/geometry mismatch");
    }
    std::set<std::string> attraction_nodes;
    for(const auto& n:root["attractions"].as_array()) {
        std::string id=n["id"].as_string(),node=n["node_id"].as_string();int idx=staged.get_place_index(node);
        if(id.empty()||staged.has_place(id)||idx<0||!attraction_nodes.insert(node).second||n["osm_feature"].as_string().empty()||n["name"].as_string().empty()||!n["snap_distance_m"].is_number()||n["snap_distance_m"].as_double()<0||n["snap_distance_m"].as_double()>500 || !std::isfinite(n["snap_distance_m"].as_double()) || !n["lon"].is_number() || !n["lat"].is_number() || n["snap_method"].as_string().empty()) throw std::runtime_error("Invalid/ambiguous catalog snap");
        auto& p=staged.get_place(idx);p.attraction=true;p.name=n["name"].as_string();
        if(schema==3){
            const auto& wd=n["crowd_weekday"];const auto& we=n["crowd_weekend"];
            if(!n["category"].is_string()||n["category"].as_string().empty()||!n["visit_minutes"].is_number()||n["visit_minutes"].as_int()<=0||n["visit_minutes"].as_int()>600||!n["open_hour"].is_number()||!n["close_hour"].is_number()||n["open_hour"].as_int()<0||n["close_hour"].as_int()>24||n["open_hour"].as_int()>=n["close_hour"].as_int()||!wd.is_array()||!we.is_array()||wd.size()!=24||we.size()!=24||!n["estimated"].is_bool()) throw std::runtime_error("Invalid v3 catalog metadata");
            p.category=n["category"].as_string();p.categories.push_back(p.category);p.visit_minutes=n["visit_minutes"].as_int();p.open_hour=n["open_hour"].as_int();p.close_hour=n["close_hour"].as_int();p.metrics_estimated=n["estimated"].as_bool();
            for(int h=0;h<24;++h){ double a=wd[h].as_double(-1),b=we[h].as_double(-1); if(a<0||a>10||b<0||b>10) throw std::runtime_error("Crowd curve out of range"); p.crowd_weekday[h]=(int)a;p.crowd_weekend[h]=(int)b; }
            p.crowd=p.crowd_weekday[12];
        }
        staged.add_alias(id,idx);staged_trie.insert(p.name,id,p.name);staged_trie.insert(id,id,p.name);
    }
    for(const auto& n:root["turn_restrictions"].as_array()) {
        const std::string via=n["via"].as_string();
        if(!staged.has_place(via)||!n["only"].is_bool()||n["from_way"].as_string().empty()||n["to_way"].as_string().empty()) throw std::runtime_error("Invalid turn rule");
        auto* rules=staged.turns.find(via);
        if(!rules){staged.turns.insert(via,ds::DynArray<ds::Graph::Turn>());rules=staged.turns.find(via);}
        rules->push_back({n["from_way"].as_string(),n["to_way"].as_string(),n["only"].as_bool()});
    }
    graph=std::move(staged);trie=std::move(staged_trie);metadata=root;return true;
 } catch(const std::exception& ex) {error=ex.what();return false;}
}
}

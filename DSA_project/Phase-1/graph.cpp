#include "graph.hpp"
#include <cmath>
#include <algorithm>
using json = nlohmann::json;

static double euclid_dist(double lat1, double lon1, double lat2, double lon2) {
    double dx = lat1 - lat2;
    double dy = lon1 - lon2;
    return std::sqrt(dx*dx + dy*dy);
}

void Graph::loadFromJson(const json &j) {
    nodes.clear();
    adj.clear();
    edge_by_id.clear();

    const auto& jnodes = j["nodes"];
    nodes.reserve(jnodes.size());

    for (const auto &jn : jnodes) {
        Node node;
        node.id = jn["id"];
        node.lat = jn.value("lat", 0.0);
        node.lon = jn.value("lon", 0.0);
        if (jn.contains("pois"))
            for (const auto &p : jn["pois"]) node.pois.push_back(p);
        nodes[node.id] = node;
    }

    // Reserve space
    const auto& jedges = j["edges"];
    edge_by_id.reserve(jedges.size());

    for (const auto &je : jedges) {
        Edge e;
        e.id = je["id"];
        e.u = je["u"];
        e.v = je["v"];
        e.length = je.value("length", 0.0);
        e.average_time = je.value("average_time", 0.0);
        if (je.contains("speed_profile"))
            for (const auto &p : je["speed_profile"]) e.speed_profile.push_back(p);
        e.oneway = je.value("oneway", false);
        e.road_type = je.value("road_type", "");
        adj[e.u].push_back(e);
        if (!e.oneway) {
            Edge er = e;
            std::swap(er.u, er.v);
            adj[er.u].push_back(er);
        }
        edge_by_id[e.id] = e;
    }
}

std::unordered_map<int, Edge> removed_edges;

bool Graph::removeEdge(int edge_id) {
    auto it = edge_by_id.find(edge_id);
    if (it == edge_by_id.end()) return false;
    Edge e = it->second;
    
    // SMALL CHANGE 15: Check if already removed
    if (removed_edges.count(edge_id)) return false;
    
    removed_edges[edge_id] = e;
    auto &vec = adj[e.u];
    vec.erase(std::remove_if(vec.begin(), vec.end(),
              [&](const Edge &x){ return x.id == edge_id; }), vec.end());
    if (!e.oneway) {
        auto &vec2 = adj[e.v];
        vec2.erase(std::remove_if(vec2.begin(), vec2.end(),
                  [&](const Edge &x){ return x.id == edge_id; }), vec2.end());
    }
    edge_by_id.erase(it);
    return true;
}

bool Graph::modifyEdge(int edge_id, const json &patch) {
    Edge e;
    auto it = edge_by_id.find(edge_id);

    if (it != edge_by_id.end() && patch.empty()) return false;

    if (it != edge_by_id.end()) {
        e = it->second;

        auto &vec = adj[e.u];
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                  [&](const Edge &x){ return x.id == edge_id; }), vec.end());
        if (!e.oneway) {
            auto &vec2 = adj[e.v];
            vec2.erase(std::remove_if(vec2.begin(), vec2.end(),
                      [&](const Edge &x){ return x.id == edge_id; }), vec2.end());
        }
    } else {
        auto it2 = removed_edges.find(edge_id);
        if (it2 == removed_edges.end()) return false;
        e = it2->second;
        removed_edges.erase(it2);
    }
    
    if (patch.contains("length")) {
        double new_length = patch["length"];
        if (new_length <= 0) return false;
        e.length = new_length;
    }
    
    if (patch.contains("average_time")) {
        double new_time = patch["average_time"];
        if (new_time <= 0) return false;
        e.average_time = new_time;
    }
    
    if (patch.contains("speed_profile")) {
        e.speed_profile.clear();
        for (const auto &sp : patch["speed_profile"])
            e.speed_profile.push_back(sp);
    }
    
    if (patch.contains("road_type")) e.road_type = patch["road_type"];

    adj[e.u].push_back(e);
    if (!e.oneway) { Edge r = e; std::swap(r.u, r.v); adj[r.u].push_back(r); }
    edge_by_id[edge_id] = e;

    return true;
}

int Graph::nearestNodeByEuclid(double lat, double lon) const {
    double bestD = 1e18;
    int best = -1;
    for (auto &[id, n] : nodes) {
        double d = euclid_dist(lat, lon, n.lat, n.lon);
        if (d < bestD) { bestD = d; best = id; }
    }
    return best;
}
#include "graph.hpp"
#include <cmath>
#include <algorithm>
using namespace std;

static double euclid_dist(double lat1, double lon1, double lat2, double lon2) {
    double dx = lat1 - lat2;
    double dy = lon1 - lon2;
    return sqrt(dx * dx + dy * dy);
}

void Graph::loadFromJson(const json &j) {
    nodes.clear(); adj.clear(); edge_by_id.clear(); removed_edges.clear();

    for (const auto &jn : j["nodes"]) {
        Node node;
        node.id = jn["id"];
        node.lat = jn.value("lat", 0.0);
        node.lon = jn.value("lon", 0.0);
        if (jn.contains("pois"))
            for (auto &p : jn["pois"]) node.pois.push_back(p);
        nodes[node.id] = node;
        adj[node.id] = {}; // ensure entry
    }

    for (const auto &je : j["edges"]) {
        Edge e;
        e.id = je["id"];
        e.u = je["u"];
        e.v = je["v"];
        e.length = je.value("length", 0.0);
        e.average_time = je.value("average_time", 0.0);
        if (je.contains("speed_profile"))
            for (auto &p : je["speed_profile"]) e.speed_profile.push_back(p);
        e.oneway = je.value("oneway", false);
        e.road_type = je.value("road_type", "");
        adj[e.u].push_back(e);
        if (!e.oneway) {
            Edge rev = e;
            swap(rev.u, rev.v);
            adj[rev.u].push_back(rev);
        }
        edge_by_id[e.id] = e;
    }
}

bool Graph::removeEdge(int edge_id) {
    auto it = edge_by_id.find(edge_id);
    if (it == edge_by_id.end()) return false;
    Edge e = it->second;
    removed_edges[edge_id] = e;

    auto &v1 = adj[e.u];
    v1.erase(remove_if(v1.begin(), v1.end(), 
             [&](auto &x){ return x.id == edge_id; }), v1.end());
    if (!e.oneway) {
        auto &v2 = adj[e.v];
        v2.erase(remove_if(v2.begin(), v2.end(), 
                 [&](auto &x){ return x.id == edge_id; }), v2.end());
    }
    edge_by_id.erase(it);
    return true;
}

bool Graph::modifyEdge(int edge_id, const json &patch) {
    Edge e;
    bool restored = false;

    auto it = edge_by_id.find(edge_id);
    if (it == edge_by_id.end()) {
        if (removed_edges.count(edge_id)) {
            e = removed_edges[edge_id];
            removed_edges.erase(edge_id);
            restored = true;
        } else {
            return false; // ✅ Edge doesn't exist
        }
    } else {
        e = it->second;
    }

    // ✅ FIX: Track if oneway status changes
    bool was_oneway = e.oneway;

    if (patch.contains("length")) e.length = patch["length"];
    if (patch.contains("average_time")) e.average_time = patch["average_time"];
    if (patch.contains("road_type")) e.road_type = patch["road_type"];
    if (patch.contains("oneway")) e.oneway = patch["oneway"];
    if (patch.contains("speed_profile")) {
        e.speed_profile.clear();
        for (auto &sp : patch["speed_profile"]) 
            e.speed_profile.push_back(sp);
    }

    // ✅ Remove old edges from adjacency lists
    adj[e.u].erase(remove_if(adj[e.u].begin(), adj[e.u].end(), 
                   [&](auto &x){ return x.id == edge_id; }), adj[e.u].end());
    adj[e.v].erase(remove_if(adj[e.v].begin(), adj[e.v].end(), 
                   [&](auto &x){ return x.id == edge_id; }), adj[e.v].end());

    // ✅ Add updated edges
    adj[e.u].push_back(e);
    if (!e.oneway) { 
        Edge rev = e; 
        swap(rev.u, rev.v); 
        adj[rev.u].push_back(rev); 
    }

    edge_by_id[edge_id] = e;
    return true;
}

int Graph::nearestNodeByEuclid(double lat, double lon) const {
    double bestD = 1e18; int best = -1;
    for (auto &[id, n] : nodes) {
        double d = euclid_dist(lat, lon, n.lat, n.lon);
        if (d < bestD) { bestD = d; best = id; }
    }
    return best;
}

bool Graph::removeEdgeBetween(int u, int v) {
    bool removed = false;
    if (adj.find(u) == adj.end()) return false;
    
    auto &vec = adj[u];
    vec.erase(remove_if(vec.begin(), vec.end(), 
              [&](auto &x){ 
                  if (x.v==v){
                      removed=true;
                      return true;
                  } 
                  return false;
              }), vec.end());
    return removed;
}
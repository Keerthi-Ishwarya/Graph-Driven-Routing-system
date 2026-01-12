#include "algorithms.hpp"
#include <unordered_set>
using json = nlohmann::json;

double compute_time_with_profile(const Edge &e, double start_time_min) {
    // If no valid profile, fallback to average_time (seconds)
    if (e.speed_profile.empty())
        return e.average_time;

    double remaining_dist = e.length; // meters
    double current_time = start_time_min; // minutes
    double total_time = 0.0; // minutes (accumulate in minutes, convert at end)

    while (remaining_dist > 1e-6) {
        int slot = static_cast<int>(floor(current_time / 15.0)) % 96;
        double slot_elapsed = fmod(current_time, 15.0);
        double time_left_in_slot = 15.0 - slot_elapsed; // minutes

        double speed = e.speed_profile[slot]; // m/s
        if (speed <= 1e-6) {
            // fallback if bad speed
            double avg_speed = e.length / e.average_time; // m/s from average
            speed = avg_speed;
        }

        double distance_possible = speed * (time_left_in_slot * 60.0); // meters in this slot

        if (distance_possible >= remaining_dist - 1e-6) {
            // finish within this slot
            double time_needed_min = (remaining_dist / speed) / 60.0; // minutes
            total_time += time_needed_min;
            remaining_dist = 0.0;
        } else {
            total_time += time_left_in_slot;
            remaining_dist -= distance_possible;
            current_time += time_left_in_slot; // move to next slot
        }
    }

    return total_time * 60.0; // convert minutes → seconds
}


// --------------------------------------------------
// Dijkstra: supports "distance" and "time" modes
// --------------------------------------------------
SPResult dijkstra(const Graph &g, int source, int target, const std::string &mode_in,
                  const std::vector<int> &forbidden_nodes,
                  const std::vector<std::string> &forbidden_road_types) {
    SPResult res{false, 0.0, {}};

    std::unordered_set<int> forbidN(forbidden_nodes.begin(), forbidden_nodes.end());
    std::unordered_set<std::string> forbidR(forbidden_road_types.begin(), forbidden_road_types.end());
    if (forbidN.count(source) || forbidN.count(target))
        return res;
     if (source == target) {
       res.possible = true;
       res.cost = 0.0;             //added source=target case
       res.path = {source};
       return res;
   }
    // normalize mode
    std::string mode = mode_in;
    std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);

    if (g.nodes.find(source) == g.nodes.end() || 
    g.nodes.find(target) == g.nodes.end()) {
    return res;  // Source or target doesn't exist
    }
    const double INF = std::numeric_limits<double>::infinity();
    std::unordered_map<int, double> dist;
    std::unordered_map<int, int> parent; 
    for (auto &[id, _] : g.nodes) dist[id] = INF;
    dist[source] = 0.0;

    using P = std::pair<double, int>;
    std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
    pq.push({0.0, source});

    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (d > dist[u]) continue;
        if (u == target) break;
        if (forbidN.count(u)) continue;
        if (!g.adj.count(u)) continue;

        for (const auto &e : g.adj.at(u)) {
            if (forbidR.count(e.road_type) || forbidN.count(e.v)) continue;

            double w;
            if (mode == "time") {
                double start_time_min = dist[u]/60;
                if (!e.speed_profile.empty())
                    w = compute_time_with_profile(e, start_time_min);//in seconds
                else
                    w = e.average_time;
            } else {
                w = e.length; // distance mode
            }

            if (dist[u] + w < dist[e.v]) {
                dist[e.v] = dist[u] + w;
                parent[e.v] = u;
                pq.push({dist[e.v], e.v});
            }
        }
    }

    if (dist[target] == INF)
        return res;

    // reconstruct path
    std::vector<int> path;
    for (int cur = target;;) {
        path.push_back(cur);
        if (cur == source) break;
        cur = parent[cur];
    }
    reverse(path.begin(), path.end());

    res.possible = true;
    res.cost = dist[target];
    res.path = path;
    return res;
}
std::vector<int> knn_euclid(const Graph &g, const json &query, int k) {
    double qlat = query["query_point"]["lat"];
    double qlon = query["query_point"]["lon"];
    //changed type to poi_type in here
    std::string p_type = query["poi"];
    struct Item { double d; int id; };
    std::vector<Item> v;
    for (auto &[id, n] : g.nodes) {
        bool has = false;
        for (auto &p : n.pois) if (p == p_type) has = true;
        if (!has) continue;

        double d = hypot(n.lat - qlat, n.lon - qlon);
        v.push_back({d, id});
    }
    sort(v.begin(), v.end(), [](auto &a, auto &b){ return a.d < b.d; });
    std::vector<int> out;
    for (int i = 0; i < (int)v.size() && (int)out.size() < k; i++) out.push_back(v[i].id);
    return out;
}

std::vector<int> knn_shortest_path(const Graph &g, const json &query, int k) {
    double qlat = query["query_point"]["lat"];
    double qlon = query["query_point"]["lon"];
    //changed type to poi_type in here
    std::string p_type = query["poi"];
    //start variable now has the id value of the nearest vertex
    int start = g.nearestNodeByEuclid(qlat, qlon);
    if (start == -1) return {};
    const double INF = std::numeric_limits<double>::infinity();
    std::unordered_map<int,double> dist;
    for (auto &[id,_] : g.nodes) dist[id] = INF; 
    dist[start] = 0.0;
    using P = std::pair<double,int>;
    std::priority_queue<P,std::vector<P>,std::greater<P>> pq;
    pq.push({0.0,start});
    while(!pq.empty()){
        auto [d,u]=pq.top(); pq.pop();
        if(d>dist[u])continue;
        if (!g.adj.count(u)) continue;
        for(auto &e:g.adj.at(u)){
            if(dist[u]+e.length<dist[e.v]){
                dist[e.v]=dist[u]+e.length;
                pq.push({dist[e.v],e.v});
            }
        }
    }
    struct R{double d;int id;};
    std::vector<R> found;
    for(auto &[id,n]:g.nodes){
        bool has=false;
        for(auto &p:n.pois)if(p==p_type)has=true;
        if(!has)continue;
        if(dist[id]<INF)found.push_back({dist[id],id});
    }
    sort(found.begin(),found.end(),[](auto&a,auto&b){return a.d<b.d;});
    std::vector<int>out;
    for(int i=0;i<(int)found.size()&&out.size()<k;i++)out.push_back(found[i].id);
    return out;
}

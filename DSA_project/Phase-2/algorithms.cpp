#include "algorithms.hpp"
#include <queue>
using namespace std;

SPResult dijkstra(const Graph &g, int source, int target) {
    SPResult res{false, 0.0, {}};
    if (g.nodes.find(source) == g.nodes.end() || g.nodes.find(target) == g.nodes.end())
        return res;

    const double INF = numeric_limits<double>::infinity();
    unordered_map<int,double> dist;
    unordered_map<int,int> parent;

    for (auto &[id,_] : g.nodes) dist[id] = INF;
    dist[source] = 0.0;

    using P = pair<double,int>;
    priority_queue<P, vector<P>, std::greater<P>> pq;
    pq.push({0.0, source});

    while (!pq.empty()) {
        auto [d,u] = pq.top(); pq.pop();
        if (d > dist[u]) continue;
        if (u == target) break;
        if (g.adj.find(u) == g.adj.end()) continue;

        for (auto &e : g.adj.at(u)) {
            double w = e.length;  

            if (dist[u] + w < dist[e.v]) {
                dist[e.v] = dist[u] + w;
                parent[e.v] = u;
                pq.push({dist[e.v], e.v});
            }
        }
    }

    if (dist[target] == INF) return res;

    vector<int> path;
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

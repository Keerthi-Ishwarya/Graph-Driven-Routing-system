#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <cmath>
#include <algorithm>
#include <limits>
#include "graph.hpp"
#include "nlohmann/json.hpp"
using namespace std;
using json = nlohmann::json;

static const double INF = 1e18;

unordered_map<int,double> dijkstra_all(const Graph& g, int s)
{
    unordered_map<int,double> dist;
    for (auto &p : g.nodes) dist[p.first] = INF;
    dist[s] = 0.0;

    using State = pair<double,int>;
    priority_queue<State, vector<State>, greater<State>> pq;
    pq.push({0.0, s});

    while (!pq.empty()) {
        auto [d,u] = pq.top(); pq.pop();
        if (d > dist[u]) continue;

        auto it = g.adj.find(u);
        if (it == g.adj.end()) continue;

        for (const auto &e : it->second) {
            double nd = d + e.average_time;
            if (nd < dist[e.v]) {
                dist[e.v] = nd;
                pq.push({nd, e.v});
            }
        }
    }
    return dist;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        cerr << "Usage: ./precompute graph.json queries.json precomputed.bin\n";
        return 1;
    }

    Graph g;
    if (!load_graph(argv[1], g)) {
        cerr << "Failed to load graph\n";
        return 1;
    }

    ifstream f(argv[2]);
    if (!f) {
        cerr << "Failed to open queries file\n";
        return 1;
    }

    json q;
    f >> q;

    int depot = q["fleet"]["depot_node"];

    unordered_set<int> important_set;
    important_set.insert(depot);
    for (auto &o : q["orders"]) {
        important_set.insert(o["pickup"].get<int>());
        important_set.insert(o["dropoff"].get<int>());
    }

    vector<int> all_node_ids;
    for (auto &p : g.nodes) {
        all_node_ids.push_back(p.first);
    }
    sort(all_node_ids.begin(), all_node_ids.end());
    int N = all_node_ids.size();

    unordered_map<int,int> node_to_col;
    for (int i = 0; i < N; ++i) {
        node_to_col[all_node_ids[i]] = i;
    }

    vector<int> important_nodes(important_set.begin(), important_set.end());
    sort(important_nodes.begin(), important_nodes.end());
    int M = important_nodes.size();

    cout << "Computing distances for " << M << " important nodes to " << N << " total nodes...\n";

    vector<vector<double>> dist_table(M, vector<double>(N, INF));

    for (int i = 0; i < M; ++i) {
        auto distances = dijkstra_all(g, important_nodes[i]);
        
        for (auto &[node_id, dist] : distances) {
            if (node_to_col.count(node_id)) {
                dist_table[i][node_to_col[node_id]] = dist;
            }
        }
        
        if ((i + 1) % 10 == 0) {
            cout << "Processed " << (i + 1) << "/" << M << " nodes\n";
        }
    }

    vector<double> radius(N);
    vector<double> angle(N);
    
    if (g.nodes.find(depot) == g.nodes.end()) {
        cerr << "Error: Depot node " << depot << " not found in graph!\n";
        return 1;
    }
    
    auto &depot_node = g.nodes.at(depot);

    for (int i = 0; i < N; ++i) {
        auto &node = g.nodes.at(all_node_ids[i]);
        double dx = node.lat - depot_node.lat;
        double dy = node.lon - depot_node.lon;
        radius[i] = sqrt(dx * dx + dy * dy);
        angle[i] = atan2(dy, dx);
    }

    ofstream out(argv[3], ios::binary);
    if (!out) {
        cerr << "Failed to open output file\n";
        return 1;
    }

    int32_t M_int = M, N_int = N;
    
    out.write((char*)&M_int, sizeof(int32_t));
    out.write((char*)&N_int, sizeof(int32_t));
    
    out.write((char*)important_nodes.data(), M * sizeof(int32_t));
    out.write((char*)all_node_ids.data(), N * sizeof(int32_t));
    
    for (int i = 0; i < M; i++) {
        out.write((char*)dist_table[i].data(), N * sizeof(double));
    }
    
    out.write((char*)radius.data(), N * sizeof(double));
    out.write((char*)angle.data(), N * sizeof(double));
    out.close();

    cout << "Precomputation complete!\n";
    cout << "Important nodes: " << M << "\n";
    cout << "Total nodes: " << N << "\n";
    cout << "Output file: " << argv[3] << "\n";

    return 0;
}
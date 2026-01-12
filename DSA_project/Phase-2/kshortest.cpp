#include "kshortest.hpp"
#include <queue>
#include <unordered_set>
#include <set>
#include <algorithm>
using namespace std;

static double calculate_edge_overlap(const vector<int> &path1, const vector<int> &path2) {
    if (path1.size() <= 1 || path2.size() <= 1) return 0.0;
    unordered_set<long long> edges1, edges2;
    edges1.reserve(path1.size());
    edges2.reserve(path2.size());

    auto encode = [](int u, int v){
        return ( (long long)u << 32 ) | v; //combining 2 ints into single 64bit no.
    };               //t store edges in  hash set easily

    for (size_t i = 0; i + 1 < path1.size(); ++i)
        edges1.insert(encode(path1[i], path1[i + 1]));
    for (size_t i = 0; i + 1 < path2.size(); ++i)
        edges2.insert(encode(path2[i], path2[i + 1]));

    int common = 0;
    for (const auto &e : edges1)
        if (edges2.count(e)) common++;

    size_t total_edges = min(edges1.size(), edges2.size());
    if (total_edges == 0) return 0.0;

    return 100.0 * common / total_edges;
}

vector<PathResult> yen_k_shortest_paths(const Graph &g, int src, int tgt, int k) {
    vector<PathResult> A;

    auto first = dijkstra(g, src, tgt);
    if (!first.possible)
        return {};

    A.push_back({first.path, first.cost});

    using Candidate = pair<double, vector<int>>;
    priority_queue<Candidate, vector<Candidate>, greater<Candidate>> B;
    set<vector<int>> visited;

    for (int k_idx = 1; k_idx < k; ++k_idx) {
        const vector<int> &prev_best = A[k_idx - 1].path;

        for (int i = 0; i < (int)prev_best.size() - 1; ++i) {
            int spur = prev_best[i];
            vector<int> root(prev_best.begin(), prev_best.begin() + i + 1);

            double root_cost = 0.0;
            for (int r = 0; r < i; ++r) {
                int u = prev_best[r];
                int v = prev_best[r + 1];
                if (g.adj.find(u) != g.adj.end()) {
                    for (auto &e : g.adj.at(u)) {
                        if (e.v == v) {
                            root_cost += e.length;
                            break;
                        }
                    }
                }
            }

            Graph g_copy = g;

            // Remove edges that share the same root path
            for (const auto &p : A) {
                if ((int)p.path.size() > i && 
                    equal(root.begin(), root.end(), p.path.begin())) {
                    g_copy.removeEdgeBetween(p.path[i], p.path[i + 1]);
                }
            }

            // Remove root path nodes (except spur node)
            for (int r = 0; r < i; ++r) {
                int node = prev_best[r];
                // Remove all edges from this node
                if (g_copy.adj.find(node) != g_copy.adj.end()) {
                    g_copy.adj[node].clear();
                }
                // Remove all edges TO this node
                for (auto &[n, edges] : g_copy.adj) {
                    edges.erase(remove_if(edges.begin(), edges.end(),
                                [node](const Edge& e) { return e.v == node; }),
                               edges.end());
                }
            }

            auto spur_res = dijkstra(g_copy, spur, tgt);
            if (!spur_res.possible)
                continue;

            vector<int> total = root;
            total.insert(total.end(), spur_res.path.begin() + 1, spur_res.path.end());

            if (visited.count(total))
                continue;

            visited.insert(total);

            double total_cost = root_cost + spur_res.cost;
            if (total_cost < 1e18){
                B.push({total_cost, total});
                // REMOVED THE EARLY BREAK HERE
            }
        }

        if (B.empty())
            break;

        auto [cost, path] = B.top();
        B.pop();
        A.push_back({path, cost});
    }

    return A;
}

vector<PathResult> heuristic_k_shortest_paths(const Graph &g, int src, int tgt, int k, double overlap_threshold) {
    vector<PathResult> results;

    auto base = dijkstra(g, src, tgt);
    if (!base.possible)
        return {};

    results.push_back({base.path, base.cost});

    unordered_map<int, int> edge_usage;

    for (size_t i = 0; i + 1 < base.path.size(); ++i) {
        int u = base.path[i];
        int v = base.path[i + 1];
        if (g.adj.find(u) != g.adj.end()) {
            for (auto &e : g.adj.find(u)->second) {
                if (e.v == v) {
                    edge_usage[e.id]++;
                    break;
                }
            }
        }
    }

    for (int ki = 1; ki < k; ++ki) {
        Graph mod = g;

        for (auto &[id, e] : mod.edge_by_id) {
            if (edge_usage.count(id)) {
                for (auto &edge : mod.adj[e.u]) {
                    if (edge.id == id)
                        edge.length *= (1.0 + 0.3 * edge_usage[id]);
                }
                if (!e.oneway) {
                    for (auto &edge : mod.adj[e.v]) {
                        if (edge.id == id)
                            edge.length *= (1.0 + 0.3 * edge_usage[id]);
                    }
                }
            }
        }

        auto res = dijkstra(mod, src, tgt);
        if (!res.possible) break;

        bool acceptable = true;
        for (auto &prev : results) {
            double overlap = calculate_edge_overlap(prev.path, res.path);
            if (overlap > overlap_threshold) {
                acceptable = false;
                break;
            }
        }

        if (!acceptable) {
            for (size_t i = 0; i + 1 < res.path.size(); ++i) {
                int u = res.path[i];
                int v = res.path[i + 1];
                if (g.adj.find(u) != g.adj.end()) {
                    for (auto &e : g.adj.find(u)->second) {
                        if (e.v == v) {
                            edge_usage[e.id] += 2;
                            break;
                        }
                    }
                }
            }
            continue;
        }

        bool is_duplicate = false;
        for (auto &p : results) {
            if (p.path == res.path) {
                is_duplicate = true;
                break;
            }
        }
        if (is_duplicate) continue;

        results.push_back({res.path, res.cost});

        for (size_t i = 0; i + 1 < res.path.size(); i++) {
            int u = res.path[i];
            int v = res.path[i + 1];
            if (g.adj.find(u) != g.adj.end()) {
                for (auto &e : g.adj.find(u)->second) {
                    if (e.v == v) {
                        edge_usage[e.id]++;
                        break;
                    }
                }
            }
        }
    }

    return results;
}
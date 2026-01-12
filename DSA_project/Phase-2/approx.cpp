#include "approx.hpp" 
#include <chrono>
#include <queue>
#include <cmath>
#include <unordered_set>

using namespace std;

static double heuristic(const Graph &g, int current, int target) {
    if (g.nodes.find(current) == g.nodes.end() || 
        g.nodes.find(target) == g.nodes.end()) {
        return 0.0;
    }
    
    auto &n1 = g.nodes.at(current);
    auto &n2 = g.nodes.at(target); 

    double dx = n1.lat - n2.lat;
    double dy = n1.lon - n2.lon;

    return sqrt(dx * dx + dy * dy);
}

// Weighted A* with epsilon for approximation
static double weighted_astar(const Graph &g,
                             int source,
                             int target,
                             double epsilon,
                             chrono::steady_clock::time_point start_all,
                             double total_time_budget_ms)
{
    const double INF = 1e18;
    
    // Check if nodes exist
    if (g.nodes.find(source) == g.nodes.end() || 
        g.nodes.find(target) == g.nodes.end()) {
        return -1;
    }
    
    // Same node
    if (source == target) {
        return 0.0;
    }

    unordered_map<int, double> g_score;  // Actual distance from source
    unordered_map<int, double> f_score;  // g + weighted h
    unordered_set<int> closed;
    
    for (auto &[id, node] : g.nodes) {
        g_score[id] = INF;
        f_score[id] = INF;
    }
    
    g_score[source] = 0.0;
    double h_source = heuristic(g, source, target);
    f_score[source] = (1.0 + epsilon) * h_source;  // Weighted heuristic

    using State = pair<double, int>;  // (f_score, node)
    priority_queue<State, vector<State>, greater<State>> pq;
    pq.push({f_score[source], source});

    while (!pq.empty()) {
        // Check time budget
        auto now = chrono::steady_clock::now();
        double elapsed = chrono::duration<double, milli>(now - start_all).count();
        if (elapsed > total_time_budget_ms * 0.95) {  // Leave 5% margin
            return -1;
        }

        auto [f, u] = pq.top();
        pq.pop();

        // Skip if already processed
        if (closed.count(u)) continue;
        closed.insert(u);

        // Found target
        if (u == target) {
            return g_score[u];
        }

        // Skip if this is an outdated entry
        if (f > f_score[u]) continue;

        if (g.adj.find(u) == g.adj.end()) continue;

        for (auto &e : g.adj.at(u)) {
            int v = e.v;
            
            // Skip if already closed
            if (closed.count(v)) continue;

            double tentative_g = g_score[u] + e.length;

            if (tentative_g < g_score[v]) {
                g_score[v] = tentative_g;
                double h_v = heuristic(g, v, target);
                f_score[v] = tentative_g + (1.0 + epsilon) * h_v;  // Weighted A*
                pq.push({f_score[v], v});
            }
        }
    }

    // No path found
    return (g_score[target] >= INF) ? -1 : g_score[target];
}

vector<ApproxResult> approx_batch(const Graph &g, 
                                  const json &queries, 
                                  double time_budget_ms, 
                                  double error_pct)
{
    vector<ApproxResult> out;

    auto start_all = chrono::steady_clock::now();
    
    // Convert error percentage to epsilon for weighted A*
    // For error_pct = 5, we want paths within 5% of optimal
    // Weighted A* with weight w = 1 + epsilon guarantees (1+epsilon) approximation
    // So if we want 5% error, epsilon should be 0.05
    double epsilon = error_pct / 100.0;

    for (auto &q : queries) {
        // Check if we've exceeded time budget
        double elapsed = chrono::duration<double, milli>(
            chrono::steady_clock::now() - start_all
        ).count();

        if (elapsed > time_budget_ms) {
            break;  // Stop processing more queries
        }

        int s = q["source"];
        int t = q["target"];

        // Validate nodes exist
        if (g.nodes.find(s) == g.nodes.end() || 
            g.nodes.find(t) == g.nodes.end()) {
            continue;
        }

        double approx_dist = weighted_astar(g, s, t, epsilon, start_all, time_budget_ms);

        if (approx_dist >= 0) {
            out.push_back({s, t, approx_dist});
        }
    }
    
    return out;
}
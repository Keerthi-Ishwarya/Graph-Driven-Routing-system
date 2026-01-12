#include "graph.hpp"
#include "nlohmann/json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;
using namespace std;

bool load_graph(const std::string& filename, Graph& g)
{
    ifstream fin(filename);
    if (!fin) {
        cerr << "Could not open graph file: " << filename << "\n";
        return false;
    }

    json j;
    try {
        fin >> j;
    } catch (const exception& e) {
        cerr << "Error parsing JSON: " << e.what() << "\n";
        return false;
    }

    g.nodes.clear();
    g.adj.clear();

    if (j.contains("nodes")) {
        for (auto &nd : j["nodes"]) {
            Node n;
            n.id  = nd["id"];
            n.lat = nd["lat"];
            n.lon = nd["lon"];
            g.nodes[n.id] = n;
        }
    }

    if (j.contains("edges")) {
        for (auto &ed : j["edges"]) {
            int u = ed["u"];
            int v = ed["v"];

            Edge e_forward;
            e_forward.v = v;
            e_forward.length = ed["length"];
            e_forward.average_time = ed.contains("average_time") ? 
                                     (double)ed["average_time"] : e_forward.length;

            g.adj[u].push_back(e_forward);
            
            bool oneway = ed.contains("oneway") && ed["oneway"].get<bool>();
            
            if (!oneway) {
                Edge e_backward;
                e_backward.v = u;
                e_backward.length = e_forward.length;
                e_backward.average_time = e_forward.average_time;
                g.adj[v].push_back(e_backward);
            }
        }
    }

    return true;
}
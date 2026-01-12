#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include "nlohmann/json.hpp"
using json = nlohmann::json;

struct Edge {
    int id, u, v;
    double length;
    double average_time;
    std::vector<double> speed_profile;
    bool oneway;
    std::string road_type;
};

struct Node {
    int id;
    double lat, lon;
    std::vector<std::string> pois;
};

class Graph {
public:
    std::unordered_map<int, Node> nodes;
    std::unordered_map<int, std::vector<Edge>> adj;
    std::unordered_map<int, Edge> edge_by_id;
    std::unordered_map<int, Edge> removed_edges;

    void loadFromJson(const json &j);
    bool removeEdge(int edge_id);
    bool modifyEdge(int edge_id, const json &patch);
    int nearestNodeByEuclid(double lat, double lon) const;
    bool removeEdgeBetween(int u, int v);
};
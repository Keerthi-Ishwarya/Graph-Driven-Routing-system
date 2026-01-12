#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "nlohmann/json.hpp"

struct Edge {
    int id;
    int u, v;
    double length;       // meters
    double average_time; // seconds
    std::vector<double> speed_profile; 
    bool oneway;
    std::string road_type;
    bool is_removed = false;  // ADD THIS LINE
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

    void loadFromJson(const nlohmann::json &j);
    bool removeEdge(int edge_id);
    bool modifyEdge(int edge_id, const nlohmann::json &patch);
    int nearestNodeByEuclid(double lat, double lon) const;
};
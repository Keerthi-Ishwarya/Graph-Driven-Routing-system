#pragma once
#include <unordered_map>
#include <vector>
#include <string>

struct Node {
    int id;
    double lat;
    double lon;
};

struct Edge {
    int v;
    double length;
    double average_time;
};

struct Graph {
    std::unordered_map<int, Node> nodes;
    std::unordered_map<int, std::vector<Edge>> adj;
};

bool load_graph(const std::string& filename, Graph& g);

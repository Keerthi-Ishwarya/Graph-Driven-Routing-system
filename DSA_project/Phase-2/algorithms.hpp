#pragma once
#include "graph.hpp"
#include <vector>
#include <unordered_map>
struct SPResult {
    bool possible;
    double cost;
    std::vector<int> path;
};

SPResult dijkstra(const Graph &g, int source, int target);


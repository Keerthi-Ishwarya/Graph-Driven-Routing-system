#pragma once
#include "graph.hpp"
#include <vector>
#include <string>
#include <optional>
#include <queue>
#include <limits>

struct SPResult {
    bool possible;
    double cost;
    std::vector<int> path;
};

SPResult dijkstra(const Graph &g, int source, int target, const std::string &mode,
                  const std::vector<int> &forbidden_nodes,
                  const std::vector<std::string> &forbidden_road_types);

std::vector<int> knn_euclid(const Graph &g, const nlohmann::json &query, int k);
std::vector<int> knn_shortest_path(const Graph &g, const nlohmann::json &query, int k);

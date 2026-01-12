#pragma once
#include "algorithms.hpp"

struct PathResult {
    std::vector<int> path;
    double length;
};

std::vector<PathResult> yen_k_shortest_paths(const Graph &g, int src, int tgt, int k);
std::vector<PathResult> heuristic_k_shortest_paths(const Graph &g, int src, int tgt, int k, double overlap_threshold);
#pragma once
#include "algorithms.hpp"

struct ApproxResult {
    int source, target;
    double approx_shortest_distance;
};

std::vector<ApproxResult> approx_batch(const Graph &g, const json &queries, double time_budget_ms, double error_pct);

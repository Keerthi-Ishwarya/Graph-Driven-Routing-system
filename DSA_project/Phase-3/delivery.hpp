#pragma once
#include "graph.hpp"
#include <vector>

struct Order {
    int order_id;
    int pickup;
    int dropoff;
};

struct DriverAssignment {
    int driver_id;
    std::vector<int> route;
    std::vector<int> order_ids;
};

bool load_precomputed(const std::string& file);

std::vector<DriverAssignment> schedule_deliveries(
    const Graph& g,
    const std::vector<Order>& orders,
    int num_drivers,
    int depot_node
);

double compute_total_delivery_time(
    const Graph& g,
    const std::vector<DriverAssignment>& assignments,
    const std::vector<Order>& orders
);

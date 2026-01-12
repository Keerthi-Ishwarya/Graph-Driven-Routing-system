#include "delivery.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <cmath>
#include <algorithm>
#include <limits>
#include <set>
using namespace std;

static vector<int> important_nodes;
static vector<int> all_nodes;
static unordered_map<int,int> id_to_row;
static unordered_map<int,int> id_to_col;
static vector<vector<double>> distTable;
static vector<double> radius_vals;
static vector<double> angle_vals;

bool load_precomputed(const string &file)
{
    ifstream fin(file, ios::binary);
    if (!fin) return false;

    int M, N;
    fin.read((char*)&M, sizeof(int));
    fin.read((char*)&N, sizeof(int));

    important_nodes.resize(M);
    fin.read((char*)important_nodes.data(), M * sizeof(int));

    all_nodes.resize(N);
    fin.read((char*)all_nodes.data(), N * sizeof(int));

    id_to_row.clear();
    id_to_col.clear();
    
    for (int i = 0; i < M; i++)
        id_to_row[important_nodes[i]] = i;
    
    for (int i = 0; i < N; i++)
        id_to_col[all_nodes[i]] = i;

    distTable.assign(M, vector<double>(N));
    for (int i = 0; i < M; i++)
        fin.read((char*)distTable[i].data(), N * sizeof(double));

    radius_vals.resize(N);
    angle_vals.resize(N);

    fin.read((char*)radius_vals.data(), N * sizeof(double));
    fin.read((char*)angle_vals.data(), N * sizeof(double));

    return true;
}

static double shortest_time(int u, int v)
{
    if (u == v) return 0.0;
    if (id_to_row.find(u) == id_to_row.end()) return 1e18;
    if (id_to_col.find(v) == id_to_col.end()) return 1e18;
    
    return distTable[id_to_row[u]][id_to_col[v]];
}

static bool is_valid_route(const vector<int>& route, const vector<Order>& orders) {
    unordered_map<int, bool> picked_up;
    unordered_map<int, bool> delivered;
    
    for (const auto& order : orders) {
        picked_up[order.order_id] = false;
        delivered[order.order_id] = false;
    }
    
    for (int node : route) {
        for (const auto& order : orders) {
            if (node == order.pickup) {
                picked_up[order.order_id] = true;
            }
            if (node == order.dropoff) {
                if (!picked_up[order.order_id]) {
                    return false;
                }
                delivered[order.order_id] = true;
            }
        }
    }
    
    for (const auto& order : orders) {
        if (!picked_up[order.order_id] || !delivered[order.order_id]) {
            return false;
        }
    }
    
    return true;
}

static vector<int> greedy_route(const vector<Order>& orders, int depot) {
    if (orders.empty()) return {depot};
    
    vector<int> route = {depot};
    vector<bool> picked_up(orders.size(), false);
    vector<bool> delivered(orders.size(), false);
    int current = depot;
    int remaining = orders.size() * 2;

    while (remaining > 0) {
        double best = 1e18;
        int best_node = -1;
        int best_idx = -1;
        bool is_pickup = true;

        for (int i = 0; i < (int)orders.size(); i++) {
            if (!picked_up[i]) {
                double d = shortest_time(current, orders[i].pickup);
                if (d < best) {
                    best = d;
                    best_node = orders[i].pickup;
                    best_idx = i;
                    is_pickup = true;
                }
            }
            else if (picked_up[i] && !delivered[i]) {
                double d = shortest_time(current, orders[i].dropoff);
                if (d < best) {
                    best = d;
                    best_node = orders[i].dropoff;
                    best_idx = i;
                    is_pickup = false;
                }
            }
        }

        if (best_idx == -1) break;

        if (best_node != current || orders[best_idx].pickup == orders[best_idx].dropoff) {
            route.push_back(best_node);
            current = best_node;
        }

        if (is_pickup) {
            picked_up[best_idx] = true;
        } else {
            delivered[best_idx] = true;
        }
        remaining--;
    }

    return route;
}

static double route_cost(const vector<int>& route) {
    if (route.size() <= 1) return 0.0;
    
    double cost = 0.0;
    for (int i = 0; i < (int)route.size() - 1; i++) {
        cost += shortest_time(route[i], route[i+1]);
    }
    return cost;
}

static void two_opt_improve(vector<int>& route, const vector<Order>& orders) {
    if (route.size() <= 3) return;
    
    bool improved = true;
    int iterations = 0;
    const int MAX_ITER = 200;
    
    while (improved && iterations < MAX_ITER) {
        improved = false;
        iterations++;
        double best_cost = route_cost(route);
        
        for (int i = 1; i < (int)route.size() - 2; i++) {
            for (int j = i + 1; j < (int)route.size() - 1; j++) {
                vector<int> new_route = route;
                reverse(new_route.begin() + i, new_route.begin() + j + 1);
                
                if (is_valid_route(new_route, orders)) {
                    double new_cost = route_cost(new_route);
                    if (new_cost < best_cost - 1e-9) {
                        route = new_route;
                        best_cost = new_cost;
                        improved = true;
                        break;
                    }
                }
            }
            if (improved) break;
        }
    }
}

struct OrderInfo {
    Order order;
    double r, a;
    int idx;
};

static vector<vector<Order>> balanced_cluster(const vector<Order>& orders, int drivers) {
    if (orders.empty()) {
        return vector<vector<Order>>(drivers);
    }
    vector<OrderInfo> info;
    for (int i = 0; i < (int)orders.size(); i++) {
        auto &o = orders[i];
        if (id_to_col.find(o.pickup) == id_to_col.end()) continue;
        int pickup_col = id_to_col[o.pickup];
        OrderInfo oi{o, radius_vals[pickup_col], angle_vals[pickup_col], i};
        info.push_back(oi);
    }

    if (info.empty()) {
        return vector<vector<Order>>(drivers);
    }

    int num_angular_sectors = max(1, (int)sqrt(drivers));
    int num_radial_zones = (drivers + num_angular_sectors - 1) / num_angular_sectors;
    
    vector<double> radii;
    for (auto &oi : info) radii.push_back(oi.r);
    sort(radii.begin(), radii.end());
    
    vector<double> zone_boundaries;
    zone_boundaries.push_back(0.0);
    for (int z = 1; z < num_radial_zones; z++) {
        int idx = (radii.size() * z) / num_radial_zones;
        zone_boundaries.push_back(radii[idx]);
    }
    zone_boundaries.push_back(1e18);
    
    const double PI = acos(-1.0);
    double sector_size = 2 * PI / num_angular_sectors;
    
    vector<vector<vector<Order>>> grid(num_angular_sectors, vector<vector<Order>>(num_radial_zones));
    
    for (auto &oi : info) {
        double norm_angle = oi.a;
        while (norm_angle < 0) norm_angle += 2 * PI;
        while (norm_angle >= 2 * PI) norm_angle -= 2 * PI;
        
        int sector = min((int)(norm_angle / sector_size), num_angular_sectors - 1);
        
        int zone = 0;
        for (int z = 0; z < num_radial_zones; z++) {
            if (oi.r >= zone_boundaries[z] && oi.r < zone_boundaries[z + 1]) {
                zone = z;
                break;
            }
        }
        
        grid[sector][zone].push_back(oi.order);
    }
    
    vector<vector<Order>> clusters(drivers);
    int driver_idx = 0;
    
    for (int s = 0; s < num_angular_sectors; s++) {
        for (int z = 0; z < num_radial_zones; z++) {
            if (driver_idx >= drivers) break;
            
            for (auto &order : grid[s][z]) {
                clusters[driver_idx].push_back(order);
            }
            
            if (!grid[s][z].empty()) {
                driver_idx++;
            }
        }
        if (driver_idx >= drivers) break;
    }
    
    if (driver_idx>=0) {
        set<int> assigned_ids;
        for (auto &cluster : clusters) {
            for (auto &o : cluster) {
                assigned_ids.insert(o.order_id);
            }
        }
        
        int round_robin = 0;
        for (auto &oi : info) {
            if (assigned_ids.find(oi.order.order_id) == assigned_ids.end()) {
                clusters[round_robin % drivers].push_back(oi.order);
                round_robin++;
            }
        }
    }

    return clusters;
}

vector<DriverAssignment> schedule_deliveries(
    const Graph&,
    const vector<Order>& orders,
    int drivers,
    int depot
) {
    if (orders.empty()) {
        vector<DriverAssignment> ans(drivers);
        for (int d = 0; d < drivers; d++) {
            ans[d].driver_id = d;
            ans[d].route = {depot};
        }
        return ans;
    }
    
    auto clusters = balanced_cluster(orders, drivers);
    vector<DriverAssignment> assignments(drivers);

    for (int d = 0; d < drivers; d++) {
        assignments[d].driver_id = d;
    
        if (clusters[d].empty()) {
            assignments[d].route = {depot};
            continue;
        }
    
        for (auto &o : clusters[d]) {
            assignments[d].order_ids.push_back(o.order_id);
        }

        auto route = greedy_route(clusters[d], depot);
        assignments[d].route = route;
    
        if (is_valid_route(route, clusters[d])) {
            auto improved_route = route;
            two_opt_improve(improved_route, clusters[d]);
        
            if (is_valid_route(improved_route, clusters[d])) {
                assignments[d].route = improved_route;
            }
        }
    }
    return assignments;
}

double compute_total_delivery_time(const Graph&, const vector<DriverAssignment>& assignments, const vector<Order>& orders) {
    unordered_map<int, Order> order_map;
    for (auto &o : orders) {
        order_map[o.order_id] = o;
    }

    double total_time = 0.0;

    for (auto &driver : assignments) {
        if (driver.route.size() <= 1) continue;
        
        double elapsed = 0.0;
        unordered_set<int> picked;

        for (int i = 0; i < (int)driver.route.size() - 1; i++) {
            elapsed += shortest_time(driver.route[i], driver.route[i + 1]);
            
            int current_node = driver.route[i + 1];
            
            for (int order_id : driver.order_ids) {
                if (order_map.find(order_id) == order_map.end()) continue;
                
                auto &order = order_map[order_id];
                
                if (current_node == order.pickup) {
                    picked.insert(order_id);
                }
                else if (current_node == order.dropoff && picked.count(order_id)) {
                    total_time += elapsed;
                }
            }
        }
    }
    
    return total_time;
}
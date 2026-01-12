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
#include "graph.hpp"
#include "delivery.hpp"
#include "nlohmann/json.hpp"

using namespace std;
using json = nlohmann::json;

int main(int argc, char** argv) {
    if (argc != 4) {
        cerr << "Usage: ./phase3 graph.json queries.json output.json\n";
        return 1;
    }

    Graph g;
    if (!load_graph(argv[1], g)) {
        cerr << "Failed to load graph from " << argv[1] << "\n";
        return 1;
    }
    
    cout << "Loaded graph with " << g.nodes.size() << " nodes\n";

    if (!load_precomputed("precomputed.bin")) {
        cerr << "Failed to load precomputed data. Run ./precompute first!\n";
        return 1;
    }
    
    cout << "Loaded precomputed data\n";

    ifstream f(argv[2]);
    if (!f) {
        cerr << "Failed to open queries file " << argv[2] << "\n";
        return 1;
    }
    
    json q;
    try {
        f >> q;
    } catch (const exception& e) {
        cerr << "Error parsing queries JSON: " << e.what() << "\n";
        return 1;
    }

    vector<Order> orders;
    if (!q.contains("orders")) {
        cerr << "No orders found in queries\n";
        return 1;
    }
    
    for (auto& o : q["orders"]) {
        Order order;
        order.order_id = o["order_id"];
        order.pickup = o["pickup"];
        order.dropoff = o["dropoff"];
        orders.push_back(order);
    }
    
    cout << "Loaded " << orders.size() << " orders\n";

    int num_drivers = q["fleet"]["num_delivery_guys"];
    int depot = q["fleet"]["depot_node"];
    
    cout << "Fleet: " << num_drivers << " drivers, depot at node " << depot << "\n";

    auto start_time = chrono::high_resolution_clock::now();
    
    auto assignments = schedule_deliveries(g, orders, num_drivers, depot);
    
    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
    
    cout << "Scheduling completed in " << duration.count() << " ms\n";

    double total_time = compute_total_delivery_time(g, assignments, orders);
    
    cout << "Total delivery time: " << total_time << " seconds\n";

    json out;
    out["assignments"] = json::array();
    
    for (auto& a : assignments) {
        json assignment;
        assignment["driver_id"] = a.driver_id;
        assignment["route"] = a.route;
        assignment["order_ids"] = a.order_ids;
        out["assignments"].push_back(assignment);
    }
    
    out["metrics"] = {
        {"total_delivery_time", total_time}
    };

    ofstream out_file(argv[3]);
    if (!out_file) {
        cerr << "Failed to open output file " << argv[3] << "\n";
        return 1;
    }
    
    out_file << out.dump(2);
    out_file.close();
    
    cout << "Output written to " << argv[3] << "\n";

    return 0;
}
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
#include "nlohmann/json.hpp"
#include "graph.hpp"
#include "algorithms.hpp"


using json = nlohmann::json;
Graph G;
json process_query(const json& query) {
    json result;
    std::string type = query["type"];
    
    try {
        if (type == "remove_edge") {
            bool ok = G.removeEdge(query["edge_id"]);
            result["id"] = query["id"];
            result["done"] = ok;
            
        } else if (type == "modify_edge") {
            bool ok = G.modifyEdge(query["edge_id"], query["patch"]);
            result["id"] = query["id"];
            result["done"] = ok;
            
        } else if (type == "shortest_path") {
            int src = query["source"];
            int tgt = query["target"];
            std::string mode = query.value("mode", "distance");
            
            std::vector<int> forbidN;
            std::vector<std::string> forbidR;
            
            if (query.contains("constraints")) {
                auto c = query["constraints"];
                if (c.contains("forbidden_nodes"))
                    for (auto x : c["forbidden_nodes"]) forbidN.push_back(x);
                if (c.contains("forbidden_road_types"))
                    for (auto x : c["forbidden_road_types"]) forbidR.push_back(x);
            }
            
            SPResult r = dijkstra(G, src, tgt, mode, forbidN, forbidR);
            
            result["id"] = query["id"];
            result["possible"] = r.possible;
            if (r.possible) {
                if (mode == "time") 
                    result["minimum_time"] = r.cost;
                else 
                    result["minimum_distance"] = r.cost;
                result["path"] = r.path;
            }
            
        } else if (type == "knn") {
            int k = query["k"];
            std::string metric = query["metric"];
            
            std::vector<int> nodes = (metric == "euclidean") ? 
                knn_euclid(G, query, k) : knn_shortest_path(G, query, k);
            
            result["id"] = query["id"];
            result["nodes"] = nodes;
        }
        
    } catch (const std::exception &e) {
        result["id"] = query["id"];
        result["error"] = e.what();
    }
    
    return result;
}
int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <graph.json> <queries.json> <output.json>" << std::endl;
        return 1;
    }

    // Read graph from first file
    /*
        Add your graph reading and processing code here
        Initialize any classes and data structures needed for query processing
        Close the file after reading it
    */
     std::ifstream graph_file(argv[1]);
    if (!graph_file.is_open()) {
        std::cerr << "Failed to open " << argv[1] << std::endl;
        return 1;
    }
    
    json graph_json;
    graph_file >> graph_json;
    graph_file.close();

     G.loadFromJson(graph_json);

    // Read queries from second file
    std::ifstream queries_file(argv[2]);
    if (!queries_file.is_open()) {
        std::cerr << "Failed to open " << argv[2] << std::endl;
        return 1;
    }

    json queries_json;
    queries_file >> queries_json;
    queries_file.close();

    json meta = queries_json["meta"];
    std::vector<json> results;

    for (const auto& query : queries_json["events"]) {
        auto start_time = std::chrono::high_resolution_clock::now();

        /*
            Add your query processing code here
            Each query should return a json object which should be printed to sample.json
        */

        // Answer each query replacing the function process_query using 
        // whatever function or class methods that you have implemented
        json result = process_query(query);

        auto end_time = std::chrono::high_resolution_clock::now();
        result["processing_time"] = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        results.push_back(result);
    }

    std::ofstream output_file(argv[3]);
    if (!output_file.is_open()) {
        std::cerr << "Failed to open output.json for writing" << std::endl;
        return 1;
    }

    json output;
    output["meta"] = meta;
    output["results"] = results;
    output_file << output.dump(4) << std::endl;

    output_file.close();
    return 0;
}
#include "nlohmann/json.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>

// Add your includes
#include "graph.hpp"
#include "algorithms.hpp"
#include "kshortest.hpp"
#include "approx.hpp"

using json = nlohmann::json;

// Global graph object
Graph G;

// Process query function for Phase 2
json process_query(const json& query) {
    json result;
    result["id"] = query["id"];
    std::string type = query["type"];
    
    try {
        if (type == "k_shortest_paths") {
            int k = query["k"];
            auto paths = yen_k_shortest_paths(G, query["source"], query["target"], k);
            
            json arr = json::array();
            if (paths.empty()) {
                result["paths"] = json::array();
            } else {
                for (auto &p : paths) {
                    arr.push_back({{"path", p.path}, {"length", p.length}});
                }
                result["paths"] = arr;
            }
        }
        else if (type == "k_shortest_paths_heuristic") {
            int k = query["k"];
            double overlap = query["overlap_threshold"];
            auto paths = heuristic_k_shortest_paths(G, query["source"], query["target"], k, overlap);
            
            json arr = json::array();
            for (auto &p : paths) {
                arr.push_back({{"path", p.path}, {"length", p.length}});
            }
            result["paths"] = arr;
        }
        else if (type == "approx_shortest_path") {
            double time_budget = query["time_budget_ms"];
            double err = query["acceptable_error_pct"];
            auto res = approx_batch(G, query["queries"], time_budget, err);
            
            json arr = json::array();
            for (auto &a : res) {
                arr.push_back({
                    {"source", a.source}, 
                    {"target", a.target}, 
                    {"approx_shortest_distance", a.approx_shortest_distance}
                });
            }
            result["distances"] = arr;
        }
        
    } catch (const std::exception &e) {
        std::cerr << "Error processing query " << query["id"] << ": " << e.what() << "\n";
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
    std::ifstream graph_file(argv[1]);
    if (!graph_file.is_open()) {
        std::cerr << "Failed to open " << argv[1] << std::endl;
        return 1;
    }
    
    json graph_json;
    graph_file >> graph_json;
    graph_file.close();
    
    // Initialize graph (preprocessing - not timed)
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
        auto start_time = std::chrono::steady_clock::now();

        // Process the query
        json result = process_query(query);

        auto end_time = std::chrono::steady_clock::now();
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
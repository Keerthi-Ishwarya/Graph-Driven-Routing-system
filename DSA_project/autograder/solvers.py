import heapq
import logging
import math
from utils import load_json, save_json
from autograder import get_logger
from typing import List, Dict, Tuple, Optional
from graph import Graph

def dijkstra_shortest_path(
    graph: Graph,
    source: int,
    target: int,
    mode: str,
    forbidden_nodes=None,
    forbidden_road_types=None
):
    if forbidden_nodes is None:
        forbidden_nodes = set()
    else:
        forbidden_nodes = set(forbidden_nodes)
    
    if forbidden_road_types is None:
        forbidden_road_types = set()
    else:
        forbidden_road_types = set(forbidden_road_types)

    # If source or target itself is forbidden -> no path
    if source in forbidden_nodes or target in forbidden_nodes:
        return [], math.inf

    dist = {node: math.inf for node in graph.nodes}
    prev = {node: None for node in graph.nodes}

    dist[source] = 0.0
    heap = [(0.0, source)]

    while heap:
        d, u = heapq.heappop(heap)

        # Skip stale
        if d > dist[u]:
            continue

        for v, eid in graph.roads_adj.get(u, []):
            # skip forbidden next node
            if v in forbidden_nodes:
                continue

            edge = graph.edges[eid]

            # edge disabled?
            if edge.disabled:
                continue

            # skip forbidden road types
            if edge.road_type in forbidden_road_types:
                continue

            # compute cost
            if mode == "time":
                cost = edge.get_cross_time(dist[u])
            else:
                cost = edge.length

            nd = dist[u] + cost
            if nd < dist[v]:
                dist[v] = nd
                prev[v] = (u, eid)
                heapq.heappush(heap, (float(nd), v))

    # unreachable
    if dist[target] == math.inf:
        return [], math.inf

    # reconstruct
    path = []
    cur = target
    while cur is not None:
        path.append(cur)
        parent = prev[cur]
        if parent is None:
            break
        cur, _ = parent

    path.reverse()
    return path, dist[target]

def euclidean_distance(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    """Calculate Euclidean distance between two coordinates."""
    return math.sqrt((lat1 - lat2) ** 2 + (lon1 - lon2) ** 2)

def find_nearest_node(graph: Graph, lat: float, lon: float) -> int:
    """Find the nearest node to given coordinates using Euclidean distance."""
    min_dist = math.inf
    nearest = None
    for node_id, node in graph.nodes.items():
        dist = euclidean_distance(lat, lon, node.lat, node.lon)
        if dist < min_dist:
            min_dist = dist
            nearest = node_id
    return nearest

def knn_query(graph: Graph, query: dict) -> List[int]:
    """Find k nearest neighbors with given POI tag."""
    poi = query.get("poi")
    query_point = query.get("query_point")
    k = query.get("k", 1)
    metric = query.get("metric", "euclidean")
    
    query_lat = query_point.get("lat")
    query_lon = query_point.get("lon")
    
    # Find all nodes with the required POI
    poi_nodes = [node_id for node_id, node in graph.nodes.items() if poi in node.tags]
    
    if not poi_nodes:
        return []
    
    if metric == "euclidean":
        # Calculate Euclidean distances
        distances = []
        for node_id in poi_nodes:
            node = graph.nodes[node_id]
            dist = euclidean_distance(query_lat, query_lon, node.lat, node.lon)
            distances.append((dist, node_id))
        
        # Sort by distance and take top k
        distances.sort()
        return [node_id for _, node_id in distances[:k]]
    
    elif metric == "shortest_path":
        # Find nearest node to query point
        nearest_node = find_nearest_node(graph, query_lat, query_lon)
        
        # Calculate shortest path distances from nearest node to all POI nodes
        distances = []
        for node_id in poi_nodes:
            _, dist = dijkstra_shortest_path(graph, nearest_node, node_id, "distance")
            if dist < math.inf:
                distances.append((dist, node_id))
        
        # Sort by distance and take top k
        distances.sort()
        return [node_id for _, node_id in distances[:k]]
    
    return []

def yen_k_shortest_paths(graph: Graph, source: int, target: int, K: int = 3, mode: str = "distance") -> List[Dict]:
    """
    Returns K shortest paths (loopless) from source to target, ordered by distance/time.
    Output format: [{'path':[...], 'length': ...}, ...]
    """
    A = []  # shortest paths found
    B = []  # potential candidates

    # Step 1: shortest path
    first_path, first_cost = dijkstra_shortest_path(graph, source, target, mode)
    if not first_path:
        return []  # no path exists
    A.append({"path": first_path, "length": first_cost})

    for k in range(1, K):
        for i in range(len(A[-1]["path"]) - 1):
            spur_node = A[-1]["path"][i]
            root_path = A[-1]["path"][:i + 1]

            removed_edges = []
            for p in A:
                if len(p["path"]) > i and p["path"][:i + 1] == root_path:
                    u = p["path"][i]
                    v = p["path"][i + 1]
                    for e_v, eid in graph.roads_adj.get(u, []):
                        if e_v == v and not graph.edges[eid].disabled:
                            graph.edges[eid].disabled = True
                            removed_edges.append(eid)
                            break

            spur_path, spur_cost = dijkstra_shortest_path(graph, spur_node, target, mode)
            if spur_path:
                total_path = root_path[:-1] + spur_path
                total_cost = 0
                current_time = 0.0
                for u_, v_ in zip(total_path[:-1], total_path[1:]):
                    eids = [eid for vv, eid in graph.roads_adj.get(u_, []) if vv == v_]
                    if eids:
                        edge = graph.edges[eids[0]]
                        if mode == "time":
                            cost = edge.get_cross_time(current_time)
                            current_time += cost
                            total_cost += cost
                        else:
                            total_cost += edge.length
                if not any(p["path"] == total_path for p in B):
                    heapq.heappush(B, (total_cost, total_path))

            for eid in removed_edges:
                graph.edges[eid].disabled = False

        if not B:
            break

        cost_, path_ = heapq.heappop(B)
        A.append({"path": path_, "length": cost_})

    return A

def handle_query(graph: Graph, query: dict) -> dict:
    qid = query.get("id")
    qtype = query.get("type")

    output = {"id": qid}

    if qtype == "remove_edge":
        edge_id = query.get("edge_id")
        if edge_id in graph.edges:
            edge = graph.edges[edge_id]
            if edge.disabled:
                output["done"] = False
            else:
                edge.disabled = True
                output["done"] = True
        else:
            output["done"] = False
        return output

    elif qtype == "modify_edge":
        edge_id = query.get("edge_id")
        patch = query.get("patch", {})
        
        # If patch is empty, output done as false
        if not patch:
            output["done"] = False
            return output
        
        if edge_id in graph.edges:
            graph.edges[edge_id].apply_patch(patch)
            graph.edges[edge_id].disabled = False
            output["done"] = True
        else:
            output["done"] = False
        return output

    elif qtype == "shortest_path":
        mode = query.get("mode", "distance")
        constraints = query.get("constraints", {})
        forbidden_nodes = constraints.get("forbidden_nodes", [])
        forbidden_road_types = constraints.get("forbidden_road_types", [])
        
        path, cost = dijkstra_shortest_path(
            graph, 
            query["source"], 
            query["target"], 
            mode, 
            forbidden_nodes, 
            forbidden_road_types
        )
        
        output["possible"] = bool(path)
        if path:
            output["minimum_time/minimum_distance"] = cost
            output["path"] = path
        return output

    elif qtype == "k_shortest_paths":
        mode = query.get("mode", "distance")
        K = query.get("k", 1)
        paths = yen_k_shortest_paths(graph, query["source"], query["target"], K, mode)
        output["possible"] = bool(paths)
        output["paths"] = paths
        return output

    elif qtype == "knn":
        nodes = knn_query(graph, query)
        output["nodes"] = nodes
        return output

    else:
        output["error"] = f"unknown query type: {qtype}"
        return output


if __name__ == "__main__":
    logger = get_logger("solver", "solver.log", logging.DEBUG)
    graph_data = load_json("../Testcases/graph-1-50.json")
    graph = Graph(graph_data, logger)
    input_data = load_json("../Testcases/phase2/task1/t1.json")
    
    # Copy meta from input
    output = {"meta": input_data.get("meta", {})}
    
    results = []
    for query in input_data.get("events", []):
        result = handle_query(graph, query)
        results.append(result)
    
    output["results"] = results
    
    from phase1 import verify
    hehe = verify("../Testcases/graph-1-50.json", "../Testcases/phase2/task1/t2.json", output, "../Testcases/phase2/task1/t2_out.json", logger)
    save_json("solver_verify.json", hehe)
    save_json("solver_output.json", output)

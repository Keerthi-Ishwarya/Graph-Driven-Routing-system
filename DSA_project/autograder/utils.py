import os
import subprocess
import json
import heapq
import time

g1 = "graph-1-50.json"
g2 = "graph-2-500.json"
g3 = "graph-3-5000.json"
g4 = "graph-4-100000.json"

graph_file_mapping = {
    (1, 1, 1): g4,
    (1, 1, 2): g4,
    (1, 2, 1): g4,
    (1, 2, 2): g4,
    (2, 1, 1): g1,
    (2, 1, 2): g2,
    (2, 1, 3): g3,
    (2, 2, 1): g3,
    (2, 2, 2): g4,
    (2, 3, 1): g4,
    (3, 1, 1): g1,
    (3, 1, 2): g2,
    (3, 1, 3): g3
}


def run_command(cmd, timeout, logger, debug=False):
    start = time.time()
    try:
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout
        )
        # logger.debug(proc.stdout)
        logger.debug(proc.stderr)
    except subprocess.TimeoutExpired as e:
        logger.error("Process timed out")
        proc = subprocess.CompletedProcess(
            cmd, returncode=-1, stdout=(e.stdout or ""), stderr=(e.stderr or ""))
    except Exception as e:
        logger.exception("Failed to run subprocess")
        proc = subprocess.CompletedProcess(
            cmd, returncode=-1, stdout="", stderr=str(e))
    total_time = time.time() - start
    return proc.returncode == 0, total_time*1000


def get_parent_name(path):
    return path.split("/")[-2]


def test_files(phase, task, i, submission):
    main_tc_dir = os.path.join("..", "Testcases")
    testcase_directory = os.path.join(
        main_tc_dir, f"phase{phase}", f"task{task}")
    graph_file = os.path.join(
        main_tc_dir, graph_file_mapping[(phase, task, i)])
    input_path = os.path.join(testcase_directory, f"t{i}.json")
    output_path = os.path.join(testcase_directory, f"t{i}_out.json")
    tmp_file = os.path.join(
        "/tmp/ds/", get_parent_name(submission) + f"p{phase}t{task}i{i}.json")
    if (not os.path.exists(tmp_file)):
        with open(tmp_file, "w") as dst:
            dst.write("{}")

    return (graph_file, input_path, output_path, tmp_file)


def compute_total_processing_time(json_out, logger):
    total = 0.0
    if not isinstance(json_out, dict):
        return -1
    for out in json_out.get("results", []):
        if "processing_time" not in out:
            logger.error("processing_time missing in result: %s", out)
            return -1
        total += float(out["processing_time"])
    return total


# helper graph functions for phase 2
def load_graph(graph_path, time=False):
    with open(graph_path, 'r') as f:
        graph_data = json.load(f)

    nodes = graph_data["nodes"]
    edges = graph_data["edges"]

    graph = {node["id"]: [] for node in nodes}

    for e in edges:
        u = e["u"]
        v = e["v"]
        eid = e["id"]
        if time:
            length = e.get("average_time", 1.0)
        else:
            length = e.get("length", 1.0)
        oneway = e.get("oneway", False)

        graph[u].append({"to": v, "edge_id": eid, "length": length})
        if not oneway:
            graph[v].append({"to": u, "edge_id": eid, "length": length})

    return graph


def dijkstra(graph, source, target):
    pq = [(0, source)]
    dist = {node: float('inf') for node in graph}
    dist[source] = 0
    parent = {node: None for node in graph}
    visited = set()

    while pq:
        d, u = heapq.heappop(pq)
        if u in visited:
            continue
        visited.add(u)
        if u == target:
            break
        for edge in graph[u]:
            v = edge["to"]
            w = edge["length"]
            if dist[u] + w < dist[v]:
                dist[v] = dist[u] + w
                parent[v] = u
                heapq.heappush(pq, (dist[v], v))

    if dist[target] == float('inf'):
        return float('inf')

    path = []
    node = target
    while node is not None:
        path.append(node)
        node = parent[node]
    path.reverse()

    return dist[target]


def compute_path_length(graph, path):
    if not path or len(path) == 1:
        return 0

    total = 0

    for i in range(len(path) - 1):
        u = path[i]
        v = path[i + 1]

        found = False
        for edge in graph.get(u, []):
            if edge["to"] == v:
                total += edge["length"]
                found = True
                break

        if not found:
            return -1

    return total


def load_json(path):
    with open(path, "r") as f:
        return json.load(f)


def save_json(path, data):
    with open(path, "w") as f:
        json.dump(data, f, indent=4)

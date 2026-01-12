import json
import os
from utils import *


def load_graph_time(g_json, time=False):
    graph = {}
    nodes = set()
    for node in g_json.get("nodes", []):
        nid = node["id"]
        nodes.add(nid)
        graph.setdefault(nid, {})
    for e in g_json.get("edges", []):
        u, v = e["u"], e["v"]
        w = e.get("average_time") if time else e.get("length")
        if w is None:
            raise ValueError(f"Edge {e.get('id')} missing weight")
        graph.setdefault(u, {})[v] = float(w)
        if not e.get("oneway", False):
            graph.setdefault(v, {})[u] = float(w)
    for n in nodes:
        graph.setdefault(n, {})
    return graph, nodes


def verify_assignments(graph, nodes_set, event, result):
    if "orders" not in event:
        return False, "Input event missing 'orders'"
    orders = event["orders"]
    order_map = {}
    for o in orders:
        oid = o.get("order_id")
        if oid is None:
            return False, "An order is missing 'order_id'"
        if oid in order_map:
            return False, f"Duplicate order_id: {oid}"
        p, d = o.get("pickup"), o.get("dropoff")
        if p is None or d is None:
            return False, f"Order {oid} missing pickup/dropoff"
        order_map[oid] = (p, d)
    total_orders = set(order_map.keys())
    if "assignments" not in result:
        return False, "Output missing 'assignments'"
    assignments = result["assignments"]
    if not isinstance(assignments, list):
        return False, "'assignments' must be a list"
    coverage = {oid: 0 for oid in total_orders}
    total_penalty_time = 0.0
    for a in assignments:
        route = a.get("route")
        assigned_oids = a.get("order_ids", [])
        if route is None or not isinstance(route, list):
            return False, "Each assignment must have a 'route' list"
        for n in route:
            if n not in nodes_set:
                return False, f"Unknown node in route: {n}"
        for i in range(len(route) - 1):
            u, v = route[i], route[i + 1]
            if v not in graph.get(u, {}):
                return False, f"Invalid edge: {u} -> {v}"
        for oid in assigned_oids:
            if oid not in order_map:
                return False, f"Unknown order_id in assignment: {oid}"
            coverage[oid] += 1
        prefix = [0.0] * len(route)
        for i in range(1, len(route)):
            u, v = route[i - 1], route[i]
            prefix[i] = prefix[i - 1] + graph[u][v]
        node_positions = {}
        for idx, node in enumerate(route):
            node_positions.setdefault(node, []).append(idx)
        for oid in assigned_oids:
            pickup, dropoff = order_map[oid]
            if pickup not in node_positions:
                return False, f"Pickup {pickup} for order {oid} not in route"
            if dropoff not in node_positions:
                return False, f"Dropoff {dropoff} for order {oid} not in route"
            pickup_idx = node_positions[pickup][0]
            drop_indices = [
                idx for idx in node_positions[dropoff] if idx > pickup_idx]
            if not drop_indices:
                return False, f"Dropoff for order {oid} occurs before pickup"
            drop_idx = drop_indices[0]
            total_penalty_time += prefix[drop_idx]
    missing = [oid for oid, cnt in coverage.items() if cnt == 0]
    multiple = [oid for oid, cnt in coverage.items() if cnt > 1]
    if missing:
        return False, f"Orders not assigned: {missing}"
    if multiple:
        return False, f"Orders assigned multiple times: {multiple}"
    return True, total_penalty_time


def test_p3_task_1(submission_path, logger):
    logger.info(f"Testing Phase 3, Task 1 for submission at {submission_path}")
    final_return = [{} for _ in range(3)]
    for k in range(1, 4):
        graph_file, in_file, _, tmp_file = test_files(3, 1, k, submission_path)

        cmd = [submission_path, graph_file, in_file, tmp_file]
        success, total_time = run_command(cmd, 500, logger)
        if not success:
            logger.error(f"Submission failed/timed out for test {k}")
            return False, {}

        with open(graph_file, 'r') as f:
            g_json = json.load(f)
        with open(in_file, 'r') as f:
            input_json = json.load(f)
        with open(tmp_file, 'r') as f:
            output_json = json.load(f)

        testcase_return = {
            "penalties": [],
            "processing_time": compute_total_processing_time(output_json, logger),
            "total_time": total_time
        }

        graph, nodes_set = load_graph_time(g_json, time=True)

        events = input_json.get("events", [])
        if not isinstance(output_json, dict):
            logger.error("Output JSON is not a dictionary")
            return False, {}

        results = output_json.get("results", [])

        for ei, (evt, res) in enumerate(zip(events, results), start=1):
            ok, info_or_err = verify_assignments(graph, nodes_set, evt, res)
            if not ok:
                logger.error(f"Event {ei} failed: {info_or_err}")
                return False, {}
            testcase_return["penalties"].append(info_or_err)
        final_return[k-1] = testcase_return
    return True, final_return

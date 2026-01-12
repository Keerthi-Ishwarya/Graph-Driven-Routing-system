from dataclasses import dataclass, field
from typing import Dict, List, Set, Tuple
import math
import logging
import heapq


@dataclass
class Node:
    id: int
    lat: float
    lon: float
    tags: List[str] = field(default_factory=list)


@dataclass
class Edge:
    id: int
    u: int
    v: int
    length: float
    average_time: float
    speed_profile: List[float]
    one_way: bool
    road_type: str
    disabled: bool = False

    def apply_patch(self, patch: dict):
        for key, value in patch.items():
            if hasattr(self, key):
                setattr(self, key, value)

    def get_cross_time(self, start_time: float) -> float:
        if not self.speed_profile:
            return self.average_time

        total_distance = self.length
        distance_covered = 0.0
        time_elapsed = 0.0
        current_time = start_time

        slot_count = len(self.speed_profile)
        slot_duration_sec = 15 * 60.0

        while distance_covered < total_distance:
            slot_index = int((current_time // slot_duration_sec) % slot_count)
            speed_per_sec = self.speed_profile[slot_index]

            if speed_per_sec <= 0:
                current_time = (slot_index + 1) * slot_duration_sec
                continue

            time_to_next_slot = ((slot_index + 1) *
                                 slot_duration_sec) - current_time
            distance_possible = speed_per_sec * time_to_next_slot

            remaining_distance = total_distance - distance_covered

            if distance_possible >= remaining_distance:
                time_needed = remaining_distance / speed_per_sec
                time_elapsed += time_needed
                distance_covered += remaining_distance
                break
            else:
                distance_covered += distance_possible
                time_elapsed += time_to_next_slot
                current_time += time_to_next_slot

        return time_elapsed


ROAD_TYPE_INDEX = {
    "primary": 0,
    "secondary": 1,
    "tertiary": 2,
    "local": 3,
    "expressway": 4
}


class Graph:
    EPS = 1e-6

    def __init__(self, graph_json: dict, logger: logging.Logger):
        self.nodes: Dict[int, Node] = {}
        self.edges: Dict[int, Edge] = {}
        self.roads_adj: Dict[int, List[Tuple[int, int]]] = {}
        self.logger = logger

        self._load_nodes(graph_json)
        self._load_edges(graph_json)

    def _load_nodes(self, graph_json: dict):
        for n in graph_json.get("nodes", []):
            self.nodes[n["id"]] = Node(
                id=n["id"],
                lat=n.get("lat", 0.0),
                lon=n.get("lon", 0.0),
                tags=n.get("pois", [])
            )

    def _load_edges(self, graph_json: dict):
        for e in graph_json.get("edges", []):
            edge = Edge(
                id=e.get("id", -1),
                u=e["u"],
                v=e["v"],
                length=e.get("length", 0.0),
                average_time=e.get("average_time", 0.0),
                speed_profile=e.get("speed_profile", []),
                one_way=e.get("oneway", False),
                road_type=e.get("road_type", "local")
            )

            self._add_edge(edge)

            # add reverse edge if needed
            if not edge.one_way:
                self._add_edge(Edge(
                    **{**edge.__dict__, "u": edge.v, "v": edge.u}
                ))

    def _add_edge(self, edge: Edge):
        """Internal utility to add edge + adjacency."""
        self.edges[edge.id] = edge

        idx = ROAD_TYPE_INDEX.get(edge.road_type, 3)
        self.roads_adj.setdefault(edge.u, [])

        self.roads_adj[edge.u].append((edge.v, edge.id))

    def remove_edge_uv(self, edge_id: int) -> bool:
        """Disable edges instead of deletion. More consistent + reversible."""
        removed = False

        if edge_id in self.edges:
            self.edges[edge_id].disabled = True
            removed = True

        return removed

    def modify_edge(self, edge_id: int, patch: dict) -> bool:
        if edge_id not in self.edges:
            return False
        self.edges[edge_id].apply_patch(patch)
        self.edges[edge_id].disabled = False
        return True

    def validate_path(self, vertices: List[int],
                      forbidden_nodes: Set[int],
                      forbidden_road_types: Set[str],
                      src: int, trg: int) -> bool:
        if not vertices or len(vertices) < 2:
            return False
        if vertices[0] in forbidden_nodes or vertices[-1] in forbidden_nodes:
            return False
        if vertices[0] != src or vertices[-1] != trg:
            return False
        for u, v in zip(vertices, vertices[1:]):
            if v in forbidden_nodes:
                return False
            found = False
            for ed_v, ed_id in self.roads_adj.get(u, []):
                if ed_v == v and not self.edges[ed_id].disabled:
                    if self.edges[ed_id].road_type in forbidden_road_types:
                        return False
                    found = True
                    break
            if not found:
                return False
        return True

    def validate_shortest_path(self, query, out, expected, mode: str) -> Tuple[bool, str]:
        if "possible" not in out:
            return False, "missing 'possible' field"

        possible = out["possible"]
        path = out.get("path", [])
        src = query.get("source")
        trg = query.get("target")

        forbidden_nodes = set(
            query.get("constraints", {}).get("forbidden_nodes", []))
        forbidden_roads = set(
            query.get("constraints", {}).get("forbidden_road_types", []))

        if not possible:
            if expected.get("possible", True):
                return False, "expected possible path, but got impossible"
            else:
                return True, ""

        if not self.validate_path(path,
                                  forbidden_nodes,
                                  forbidden_roads,
                                  src, trg):
            return False, "path validation failed"

        total = 0.0
        for u, v in zip(path, path[1:]):
            found = False
            for ed_v, ed_id in self.roads_adj.get(u, []):
                if ed_v == v and not self.edges[ed_id].disabled:
                    if self.edges[ed_id].road_type in forbidden_roads:
                        return False, "used forbidden road type"
                    if mode == "time":
                        # self.logger.debug(
                        #     f"Edge {ed_id} from {u} to {v} cross time calc at {total}")
                        total += self.edges[ed_id].get_cross_time(total)
                    else:
                        total += self.edges[ed_id].length
                    found = True
                    break
            if not found:
                return False, "edge in path not found or disabled"

        reported = out.get("minimum_time/minimum_distance")
        if reported is None:
            reported = out.get("minimum_time") if mode == "time" else out.get(
                "minimum_distance")
            if reported is None:
                return False, "missing 'minimum_time/minimum_distance' field"

        if abs(total - reported) > 1e-2:
            return False, "reported value is incorrect, computed: {}, reported: {}".format(total, reported)

        exp_total = expected.get("minimum_time/minimum_distance", 0.0)
        if exp_total - total > 1e-1:
            self.logger.info(
                f"[ERROR] Student's path is better than expected! Student's: {total}, Expected: {exp_total}")
            return True, "OH NO! Student's path is better than expected!"
        if total - exp_total > 1e-1:
            return False, "expected value is incorrect"

        return True, "Shortest path is correct along with the report time"

    def verify(self, query, out, expected) -> Tuple[bool, str]:
        t = query.get("type")
        if t == "remove_edge":
            if self.remove_edge_uv(query["edge_id"]) == out.get("done", True):
                return True, "Removed edge successfully"
            else:
                return False, "failed to remove edge"

        if t == "modify_edge":
            if self.modify_edge(query["edge_id"], query.get("patch", {})) == out.get("done", True):
                return True, "Modified edge successfully"
            else:
                return False, "failed to modify edge"

        if t == "shortest_path":
            mode = query.get("mode", "distance")
            return self.validate_shortest_path(query, out, expected, mode)

        if t == "knn":
            if query.get("metric") == "shortest_path":
                if sorted(out.get("nodes", [])) == sorted(expected.get("nodes")):
                    return True, "Shortest path knn works"
                else:
                    return False, "incorrect knn nodes"
            else:
                if sorted(out.get("nodes", [])) == sorted(expected.get("euclidian")):
                    return True, "Shortest path knn works with euclidian distances"
                elif sorted(out.get("nodes", [])) == sorted(expected.get("haversine")):
                    return True, "Shortest path knn works with haversine distances"
                else:
                    for i, (j, k) in enumerate(zip(out.get("nodes", []), expected.get("euclidian", []))):
                        if j != k:
                            self.logger.debug(
                                f"Mismatch at position {i}: got {j}, expected {k}")
                            break
                    return False, "incorrect knn nodes"

        return False, f"unknown query type: {t}"

    def verify_all(self, queries, outputs, expected):
        n = min(len(queries), len(outputs), len(expected))
        correct = 0

        for i in range(n):
            check, error = self.verify(queries[i], outputs[i], expected[i])
            if check:
                correct += 1
                self.logger.debug(
                    f"[PASS] Query {i} type={queries[i].get('type')}: {error}")
            else:
                self.logger.debug(
                    f"[FAIL] Query {i} type={queries[i].get('type')}: {error}")

        return correct, n


if __name__ == "__main__":
    from autograder import get_logger
    graph_file = "../Testcases/graph-2-500.json"
    outfile = "../Testcases/phase1/task2/t2_out.json"
    queryfile = "../Testcases/phase1/task2/t2.json"
    logger = get_logger("graph_test", "graph_test.log")
    verify_file = "../Testcases/out.json"
    logger.setLevel(logging.DEBUG)
    with open(graph_file, 'r') as gf:
        import json
        graph_data = json.load(gf)
    with open(queryfile, 'r') as qf:
        query_data = json.load(qf)
    with open(outfile, 'r') as of:
        output_data = json.load(of)
    with open(verify_file, 'r') as vf:
        expected_data = json.load(vf)
    graph = Graph(graph_data, logger)
    graph.verify_all(query_data["events"],
                     output_data["results"], expected_data["results"])

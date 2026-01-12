from utils import *


def compare_lengths(length1, length2):
    length1.sort()
    length2.sort()
    if len(length1) != len(length2):
        return 0
    for l1, l2 in zip(length1, length2):
        if l1 - l2 > 1e-1:
            return 0
    return 1


def test_p2_task_1(submission_path, logger):
    logger.info(f"Testing Phase 2, Task 1 for submission at {submission_path}")
    final_return = [{} for _ in range(3)]
    for k in range(1, 4):
        graph, in_file, exp_file, tmp_file = test_files(
            2, 1, k, submission_path)

        cmd = [submission_path, graph, in_file, tmp_file]
        success, total_time = run_command(cmd, 600, logger)
        if not success:
            return False, {}

        with open(tmp_file, 'r') as f:
            output = json.load(f)
        with open(in_file, 'r') as f:
            input = json.load(f)
        with open(exp_file, 'r') as f:
            expect = json.load(f)

        final_return[k-1]["total_time"] = total_time
        final_return[k-1]["processing_time"] = compute_total_processing_time(
            output, logger)

        if not 'results' in output:
            logger.error("No results in student output")
            return False, {}

        final_correct = []
        for inp, query, exp in zip(input["events"], output["results"], expect["results"]):
            lengths = []
            for path in query["paths"]:
                p = path.get("path", [])
                if abs(path['length'] - compute_path_length(load_graph(graph), p)) > 1e-2:
                    final_correct.append(0)
                elif len(p) == 0 or p[0] != inp["source"] or p[-1] != inp["target"]:
                    final_correct.append(0)
                else:
                    lengths.append(path["length"])
            final_correct.append(compare_lengths(
                lengths, exp))
            # final_correct.append(lengths)
        final_return[k-1]["correct"] = final_correct
    return True, final_return


def get_edges_from_node_list(path):
    return {(path[i], path[i+1]) for i in range(len(path)-1)}


def compute_penalty(paths, shortest_distance, overlap_threshold):
    k = len(paths)

    # Extract node lists + lengths
    node_lists = [p["path"] for p in paths]
    lengths = [p["length"] for p in paths]

    # Precompute edges for each path
    edge_sets = [get_edges_from_node_list(p) for p in node_lists]
    sizes = [len(e) for e in edge_sets]

    overlap_counts = [0] * k

    # Brute-force pairwise overlap comparison (k x k)
    for i in range(k):
        for j in range(k):
            if sizes[i] == 0:
                continue
            common = len(edge_sets[i].intersection(edge_sets[j]))
            perc = (common / sizes[i]) * 100
            if perc > overlap_threshold:
                overlap_counts[i] += 1

    total_penalty = 0.0

    for i in range(k):
        overlap_penalty = overlap_counts[i]
        distance_penalty = (
            (lengths[i] - shortest_distance) / shortest_distance) + 0.1
        total_penalty += overlap_penalty * distance_penalty

    return total_penalty


def test_p2_task_2(submission_path, logger):
    logger.info(f"Testing Phase 2, Task 2 for submission at {submission_path}")
    success = False

    final_return = [{} for _ in range(2)]
    for k in range(1, 3):
        graph, in_file, _, tmp_file = test_files(
            2, 2, k, submission_path)

        cmd = [submission_path, graph, in_file, tmp_file]
        success, total_time = run_command(cmd, 600, logger)
        if not success:
            return False, {}

        with open(tmp_file, 'r') as f:
            output = json.load(f)
        with open(in_file, 'r') as f:
            input = json.load(f)

        if not 'results' in output:
            logger.error("No results in student output")
            return False, {}
        graph_data = load_graph(graph)

        final_return[k-1]["total_time"] = total_time
        final_return[k-1]["processing_time"] = compute_total_processing_time(
            output, logger)
        final_return[k-1]["penalties"] = []
        for inp, outp in zip(input["events"], output["results"]):
            if inp["type"] == "k_shortest_paths_heuristic":
                if len(outp.get("paths", [])) != inp["k"]:
                    final_return[k-1]["penalties"].append(0)
                    output_correct = False
                    break

                src = inp["source"]
                dst = inp["target"]
                sd = dijkstra(graph_data, src, dst)

                output_correct = True
                for obj in outp.get("paths", []):
                    length = obj.get('length', None)
                    path = obj.get('path', [])
                    if length is None or not path:
                        final_return[k-1]["penalties"].append(0)
                        output_correct = False
                        break
                    elif abs(length - compute_path_length(graph_data, path)) > 1e-2:
                        final_return[k-1]["penalties"].append(0)
                        output_correct = False
                        break
                    elif path[0] != src or path[-1] != dst:
                        final_return[k-1]["penalties"].append(0)
                        output_correct = False
                        break

                if not output_correct:
                    continue

                final_return[k-1]["penalties"].append(compute_penalty(
                    outp['paths'], sd, inp['overlap_threshold']))
    return success, final_return


def test_p2_task_3(submission_path, logger):
    logger.info(f"Testing Phase 2, Task 3 for submission at {submission_path}")
    graph, in_file, exp_file, tmp_file = test_files(2, 3, 1, submission_path)

    cmd = [submission_path, graph, in_file, tmp_file]
    success, total_time = run_command(cmd, 600, logger)
    if not success:
        return False, {}

    final_return = {}
    final_return["total_time"] = total_time
    with open(in_file, 'r') as f:
        input = json.load(f)
    with open(tmp_file, 'r') as f:
        output = json.load(f)
    with open(exp_file, 'r') as f:
        exp_output = json.load(f)

    if not 'results' in output:
        logger.error("No results in student output")
        return False, {}

    final_return["query_results"] = []
    for inp, outp, exp_outp in zip(input["events"], output["results"], exp_output["results"]):
        query_result = {}
        query_result["id"] = inp["id"]
        query_result["correct"] = 0
        query_result["incorrect"] = 0

        if inp["type"] == "approx_shortest_path":
            error_percentage = inp["acceptable_error_pct"]
            for res, exp_res in zip(outp.get("distances", []), exp_outp.get("distances", [])):
                sd = exp_res["shortest_distance"]
                ad = res.get("approx_shortest_distance", None)
                if (not isinstance(ad, float)):
                    query_result["incorrect"] += 1
                    continue
                delta = abs(sd - ad)
                if (delta/sd * 100.0 <= error_percentage):
                    query_result["correct"] += 1
                else:
                    query_result["incorrect"] += 1

        query_result["processing_time"] = outp.get("processing_time")
        final_return["query_results"].append(query_result)
    return success, final_return

from utils import *
from graph import Graph

test_case_directory = "../Testcases/"
phase = "phase1"


def get_success_and_return(submission_path, graph, in_file, tmp_file, exp_file, logger):
    cmd = [submission_path, graph, in_file, tmp_file]
    success, total_time = run_command(cmd, 300, logger)
    if not success:
        return False, {}

    final_return = {}
    with open(tmp_file, 'r') as f:
        output = json.load(f)

    final_return["results"] = verify(graph, in_file, output, exp_file, logger)
    final_return["processing_time"] = compute_total_processing_time(
        output, logger)
    final_return["total_time"] = total_time

    return success, final_return


def test_p1_task_1a(submission_path, logger):
    logger.info(
        f"Testing Phase 1, Task 1a for submission at {submission_path}")
    graph, in_file, exp_file, tmp_file = test_files(1, 1, 1, submission_path)
    return get_success_and_return(submission_path, graph, in_file, tmp_file, exp_file, logger)


def test_p1_task_1b(submission_path, logger):
    logger.info(
        f"Testing Phase 1, Task 1b for submission at {submission_path}")
    graph, in_file, exp_file, tmp_file = test_files(1, 1, 2, submission_path)
    return get_success_and_return(submission_path, graph, in_file, tmp_file, exp_file, logger)


def test_p1_task_2a(submission_path, logger):
    logger.info(
        f"Testing Phase 1, Task 2a for submission at {submission_path}")
    graph, in_file, exp_file, tmp_file = test_files(1, 2, 1, submission_path)
    return get_success_and_return(submission_path, graph, in_file, tmp_file, exp_file, logger)


def test_p1_task_2b(submission_path, logger):
    logger.info(
        f"Testing Phase 1, Task 2b for submission at {submission_path}")
    graph, in_file, exp_file, tmp_file = test_files(1, 2, 2, submission_path)
    return get_success_and_return(submission_path, graph, in_file, tmp_file, exp_file, logger)


def verify(graph, in_file, student_output, expected_output, logger):
    g_json = load_json(graph)
    g = Graph(g_json, logger)

    queries = load_json(in_file).get("events", [])

    if not isinstance(student_output, dict):
        return False

    results = student_output.get("results", [])
    expected = load_json(expected_output).get("results", [])

    correct, total = g.verify_all(queries, results, expected)

    logger.info(f"Correct: {correct} / {total}")
    return {
        "correct": correct,
        "total": total
    }

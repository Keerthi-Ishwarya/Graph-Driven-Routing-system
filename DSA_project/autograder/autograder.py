import os
import sys
import argparse
import json

import logging
from logging.handlers import RotatingFileHandler

import phase1
import phase2
import phase3
from datetime import datetime


def get_logger(name="app", log_file="app.log", log_level=logging.DEBUG):
    logger = logging.getLogger(name)
    logger.setLevel(log_level)   # Capture everything

    # Prevent adding duplicate handlers when imported multiple times
    if logger.hasHandlers():
        return logger

    # Create log directory if needed
    os.makedirs("logs", exist_ok=True)

    # ---- Console Handler ----
    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.ERROR)

    # ---- File Handler with rotation ----
    file_handler = RotatingFileHandler(
        f"logs/{log_file}",
        maxBytes=5_000_000,  # 5MB per file
        backupCount=5
    )
    file_handler.setLevel(log_level)

    # ---- Formatter ----
    formatter = logging.Formatter(
        "%(asctime)s | %(name)s | %(levelname)s | %(message)s",
        "%Y-%m-%d %H:%M:%S"
    )
    console_handler.setFormatter(formatter)
    file_handler.setFormatter(formatter)

    # Add handlers
    logger.addHandler(console_handler)
    logger.addHandler(file_handler)

    return logger


function_map = {
    ("1", "1a"): phase1.test_p1_task_1a,
    ("1", "1b"): phase1.test_p1_task_1b,
    ("1", "2a"): phase1.test_p1_task_2a,
    ("1", "2b"): phase1.test_p1_task_2b,
    ("2", "1"): phase2.test_p2_task_1,
    ("2", "2"): phase2.test_p2_task_2,
    ("2", "3"): phase2.test_p2_task_3,
    ("3", "1"): phase3.test_p3_task_1,
}


def secure_run(phase, task, submission_path, logger):
    key = (str(phase), str(task))

    test_function = function_map[key]
    try:
        result, json = test_function(submission_path, logger)
        return result, json
    except Exception as e:
        logger.exception(
            f"Exception occurred while testing Phase {phase}, Task {task}: {e}")
        return False, {}


def test_phase1(submission_path, logger):
    logger.info(f"Testing Phase 1 for submission at {submission_path}")

    result1, json1 = secure_run(1, "1a", submission_path, logger)
    result2, json2 = secure_run(1, "1b", submission_path, logger)
    result3, json3 = secure_run(1, "2a", submission_path, logger)
    result4, json4 = secure_run(1, "2b", submission_path, logger)
    json_output = {
        "task_1a": json1,
        "task_1b": json2,
        "task_2a": json3,
        "task_2b": json4,
    }
    return json_output


def test_phase2(submission_path, logger):
    logger.info(f"Testing Phase 2 for submission at {submission_path}")
    result1, json1 = secure_run(2, "1", submission_path, logger)
    result2, json2 = secure_run(2, "2", submission_path, logger)
    result3, json3 = secure_run(2, "3", submission_path, logger)
    json_output = {
        "task_1": json1,
        "task_2": json2,
        "task_3": json3,
    }
    return json_output


def test_phase3(submission_path, logger):
    logger.info(f"Testing Phase 3 for submission at {submission_path}")
    result1, json1 = secure_run(3, "1", submission_path, logger)
    json_output = {
        "task_1": json1,
    }
    return json_output


if __name__ == "__main__":
    logger = get_logger(name="autograder", log_file="autograder.log")
    parser = argparse.ArgumentParser(
        description="Autograder for student submissions.")
    parser.add_argument("submission_name", type=str)
    parser.add_argument("--phase", type=int, choices=[0, 1, 2, 3], default=0,
                        help="Phase number to grade (1, 2, or 3). 0 for all phases.")
    args = parser.parse_args()

    submission_directory = "../final-evaluation"
    phase = args.phase
    if args.submission_name != "run":
        if not args.submission_name:
            print("Please provide a submission name.")
            sys.exit(1)
        submission_name = args.submission_name
        submission_directory = os.path.join(
            "../final-evaluation", submission_name)

    submission_result_json = os.path.join(submission_directory, "results.json")
    results = {
        "phase1": {},
        "phase2": {},
        "phase3": {}
    }
    if os.path.exists(submission_result_json):
        try:
            with open(submission_result_json, "r") as f:
                results = json.load(f)
                logger.info(f"Loaded results from {submission_result_json}")
        except Exception as e:
            logger.error(
                f"Failed to load existing results from {submission_result_json}: {e}")

    def current_time():
        return datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    if phase == 0 or phase == 1:
        submission_path = os.path.join(submission_directory, "phase1")
        if not os.path.exists(submission_path):
            logger.error(
                f"Error: The file '{submission_path}' does not exist.")
        else:
            results["phase1"] = test_phase1(submission_path, logger)
            results["phase1"]["last_update"] = current_time()

    if phase == 0 or phase == 2:
        submission_path = os.path.join(submission_directory, "phase2")
        if not os.path.exists(submission_path):
            logger.error(
                f"Error: The file '{submission_path}' does not exist.")
        else:
            results["phase2"] = test_phase2(submission_path, logger)
            results["phase2"]["last_update"] = current_time()

    if phase == 0 or phase == 3:
        submission_path = os.path.join(submission_directory, "phase3")
        if not os.path.exists(submission_path):
            logger.error(
                f"Error: The file '{submission_path}' does not exist.")
        else:
            results["phase3"] = test_phase3(submission_path, logger)
            results["phase3"]["last_update"] = current_time()

    with open(submission_result_json, "w") as f:
        json.dump(results, f, indent=4)
        logger.info(f"Wrote results to {submission_result_json}")

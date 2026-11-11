#!/usr/bin/env python3
"""Build and test the AI emergency-response simulator and its plan checks.

Run from any directory with: python3 test.py
Uses only Python's standard library and the project's configured C++ toolchain.
"""

from __future__ import annotations

import csv
import json
import math
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent
SCENARIOS_DIR = ROOT / "Scenarios"
REQUIRED_INPUTS = (
    "city_graph.json", "incidents.csv", "responders.csv",
    "road_probabilities.json", "traffic_probabilities.json",
    "observed_traffic.csv", "road_reports.csv",
)
PLAN_FIELDS = {"responder_id", "incident_id", "path", "time"}


def validate_plan(plan: Any, scenario: dict[str, Any]) -> list[str]:
    """Return validator-style error codes for a dispatch plan.

    This mirrors the independent-validator checks shown in the assignment's
    grading table. Route time uses the scenario's normal distance/speed model;
    deadlines are checked at both normal and 1.5x travel time.
    """
    errors: list[str] = []
    if not isinstance(plan, list) or any(
        not isinstance(item, dict) or set(item) != PLAN_FIELDS for item in plan
    ):
        return ["schema"]

    responders = scenario["responders"]
    incidents = scenario["incidents"]
    nodes = set(scenario["nodes"])
    roads = scenario["roads"]
    blocked = scenario.get("blocked_roads", set())
    seen_incidents: set[str] = set()
    responder_intervals: dict[str, list[tuple[float, float]]] = {}
    responder_locations = {key: value["location"] for key, value in responders.items()}

    for item in plan:
        responder_id = item["responder_id"]
        incident_id = item["incident_id"]
        path = item["path"]
        when = item["time"]
        if responder_id not in responders:
            errors.append("unknown_responder")
            continue
        if incident_id not in incidents:
            errors.append("unknown_incident")
            continue
        if incident_id in seen_incidents:
            errors.append("duplicate_incident")
        seen_incidents.add(incident_id)
        incident = incidents[incident_id]
        responder = responders[responder_id]

        if not isinstance(path, list) or not path:
            errors.append("empty_path")
            continue
        if not isinstance(when, (int, float)) or isinstance(when, bool) or not math.isfinite(when) or when < 0:
            errors.append("invalid_time")
            continue
        if responder["capability"] != incident["capability"]:
            errors.append("capability")
        if when < incident["known"]:
            errors.append("starts_before_known")
        if when < responder.get("available_at", 0):
            errors.append("responder_unavailable")
        if path[0] != responder_locations[responder_id]:
            errors.append("wrong_origin")
        if path[-1] != incident["location"]:
            errors.append("wrong_destination")
        if any(node not in nodes for node in path) or any(
            (source, target) not in roads for source, target in zip(path, path[1:])
        ):
            errors.append("invalid_path")
            continue
        if any((source, target) in blocked for source, target in zip(path, path[1:])):
            errors.append("blocked_road")

        route_time = sum(roads[(source, target)] for source, target in zip(path, path[1:]))
        if when + route_time > incident["deadline"]:
            errors.append("missed_deadline_optimistic")
        elif when + 1.5 * route_time > incident["deadline"]:
            errors.append("missed_deadline_pessimistic")

        # Responder is occupied from dispatch until arrival plus incident service.
        end = when + route_time + incident["service"]
        intervals = responder_intervals.setdefault(responder_id, [])
        if any(when < previous_end and end > previous_start for previous_start, previous_end in intervals):
            errors.append("overlap")
        intervals.append((when, end))
        responder_locations[responder_id] = incident["location"]

    return errors


class PlanValidatorUnitTests(unittest.TestCase):
    """Exercise every grading-table validation failure with minimal test data."""

    def setUp(self) -> None:
        self.scenario = {
            "nodes": {"A", "B", "C"},
            "roads": {("A", "B"): 10.0, ("B", "C"): 10.0, ("A", "C"): 30.0},
            "blocked_roads": set(),
            "responders": {
                "R1": {"location": "A", "capability": "medical", "available_at": 0},
                "R2": {"location": "B", "capability": "fire", "available_at": 0},
                "R3": {"location": "A", "capability": "medical", "available_at": 20},
            },
            "incidents": {
                "I1": {"location": "B", "capability": "medical", "known": 0, "deadline": 100, "service": 5},
                "I2": {"location": "C", "capability": "fire", "known": 5, "deadline": 100, "service": 5},
                "I3": {"location": "B", "capability": "medical", "known": 0, "deadline": 100, "service": 5},
            },
        }
        self.valid = {"responder_id": "R1", "incident_id": "I1", "path": ["A", "B"], "time": 0}

    def assert_code(self, code: str, plan: Any) -> None:
        self.assertIn(code, validate_plan(plan, self.scenario))

    def test_valid_plan_passes(self) -> None:
        self.assertEqual(validate_plan([self.valid], self.scenario), [])

    def test_schema_requires_exact_assignment_fields(self) -> None:
        self.assert_code("schema", [{**self.valid, "extra": True}])
        self.assert_code("schema", {"not": "a list"})

    def test_unknown_responder_and_incident(self) -> None:
        self.assert_code("unknown_responder", [{**self.valid, "responder_id": "missing"}])
        self.assert_code("unknown_incident", [{**self.valid, "incident_id": "missing"}])

    def test_duplicate_incident(self) -> None:
        self.assert_code("duplicate_incident", [self.valid, {**self.valid, "responder_id": "R3", "time": 30}])

    def test_empty_route(self) -> None:
        self.assert_code("empty_path", [{**self.valid, "path": []}])

    def test_start_time_must_be_valid_and_nonnegative(self) -> None:
        self.assert_code("invalid_time", [{**self.valid, "time": -1}])
        self.assert_code("invalid_time", [{**self.valid, "time": "now"}])

    def test_responder_must_have_required_capability(self) -> None:
        self.assert_code("capability", [{**self.valid, "responder_id": "R2"}])

    def test_assignment_cannot_start_before_incident_is_known(self) -> None:
        plan = {**self.valid, "incident_id": "I2", "responder_id": "R2", "path": ["B", "C"], "time": 0}
        self.assert_code("starts_before_known", [plan])

    def test_responder_must_be_available(self) -> None:
        plan = {**self.valid, "responder_id": "R3", "time": 0}
        self.assert_code("responder_unavailable", [plan])

    def test_route_origin_and_destination_must_match(self) -> None:
        self.assert_code("wrong_origin", [{**self.valid, "path": ["B"]}])
        self.assert_code("wrong_destination", [{**self.valid, "path": ["A", "C"]}])

    def test_every_route_edge_must_exist(self) -> None:
        self.assert_code("invalid_path", [{**self.valid, "path": ["A", "C", "B"]}])

    def test_route_must_not_use_blocked_road(self) -> None:
        self.scenario["blocked_roads"].add(("A", "B"))
        self.assert_code("blocked_road", [self.valid])

    def test_normal_and_pessimistic_deadline_failures(self) -> None:
        plan = {**self.valid, "time": 95}
        self.scenario["incidents"]["I1"]["deadline"] = 100
        self.assert_code("missed_deadline_optimistic", [plan])
        self.scenario["incidents"]["I1"]["deadline"] = 20
        plan["time"] = 5
        self.assert_code("missed_deadline_pessimistic", [plan])

    def test_same_responder_assignments_must_not_overlap(self) -> None:
        second = {"responder_id": "R1", "incident_id": "I3", "path": ["A", "B"], "time": 5}
        self.assert_code("overlap", [self.valid, second])


class SimulatorIntegrationTests(unittest.TestCase):
    """Build and run the real simulator with each supplied scenario."""

    @classmethod
    def setUpClass(cls) -> None:
        build = subprocess.run(
            ["make", "-B"], cwd=ROOT, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, timeout=180, check=False,
        )
        if build.returncode != 0:
            raise AssertionError(f"Build failed (exit {build.returncode}):\n{build.stdout}")
        cls.executable = ROOT / "run"

    def scenario_names(self) -> list[str]:
        return sorted(path.name for path in SCENARIOS_DIR.iterdir() if path.is_dir() and path.name.startswith("Scenario"))

    def test_scenario_files_and_json_inputs(self) -> None:
        scenarios = self.scenario_names()
        self.assertTrue(scenarios, "No Scenarios/Scenario* datasets were found")
        for name in scenarios:
            with self.subTest(scenario=name):
                folder = SCENARIOS_DIR / name
                for filename in REQUIRED_INPUTS:
                    self.assertTrue((folder / filename).is_file(), f"Missing {name}/{filename}")
                graph = json.loads((folder / "city_graph.json").read_text(encoding="utf-8"))
                self.assertTrue(graph.get("nodes"))
                node_ids = {node["id"] for node in graph["nodes"]}
                for road in graph["roads"]:
                    self.assertIn(road["from"], node_ids)
                    self.assertIn(road["to"], node_ids)
                for filename in ("road_probabilities.json", "traffic_probabilities.json"):
                    self.assertIsInstance(json.loads((folder / filename).read_text(encoding="utf-8")), dict)

    def test_each_scenario_produces_a_valid_plan_and_statistics(self) -> None:
        scenarios = self.scenario_names()
        self.assertTrue(scenarios, "No Scenarios/Scenario* datasets were found")
        for name in scenarios:
            with self.subTest(scenario=name), tempfile.TemporaryDirectory(prefix="ai-er-test-") as temp:
                scenario_dir = SCENARIOS_DIR / name
                result = subprocess.run(
                    [str(self.executable), str(scenario_dir)], cwd=temp, text=True,
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=90, check=False,
                )
                self.assertEqual(result.returncode, 0, f"{name} failed.\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}")
                docs = Path(temp) / "Documentation"
                plan_path, stats_path = docs / "plan.json", docs / "statistics.txt"
                self.assertTrue(plan_path.is_file(), f"{name} did not create plan.json")
                self.assertTrue(stats_path.is_file(), f"{name} did not create statistics.txt")
                plan = json.loads(plan_path.read_text(encoding="utf-8"))
                graph = json.loads((scenario_dir / "city_graph.json").read_text(encoding="utf-8"))
                scenario_data = self._scenario_data(scenario_dir, graph)
                errors = validate_plan(plan, scenario_data)
                self.assertEqual(errors, [], f"{name} plan violates validator checks: {errors}")
                stats = stats_path.read_text(encoding="utf-8")
                self.assertIn("System statistics:", stats)
                count = re.search(r"\* (\d+) incidents handled", stats)
                self.assertIsNotNone(count, "Statistics omit handled incident count")
                self.assertEqual(int(count.group(1)), len(plan))
                self.assertRegex(stats, r"\* \d+ responders used")
                self.assertRegex(stats, r"\* Average response time: [\d.eE+-]+s")
                self.assertRegex(stats, r"\* Deadlines met: \d+%")

    @staticmethod
    def _scenario_data(folder: Path, graph: dict[str, Any]) -> dict[str, Any]:
        with (folder / "responders.csv").open(newline="", encoding="utf-8") as source:
            responders = {
                row["responder_id"]: {
                    "location": row["start_location"],
                    "capability": row["capabilities"].split(";")[0],
                    "available_at": float(row["time_until_available"]) if row["availability"] == "Busy" else 0,
                }
                for row in csv.DictReader(source) if row.get("responder_id")
            }
        with (folder / "incidents.csv").open(newline="", encoding="utf-8") as source:
            incidents = {
                row["incident_id"]: {
                    "location": row["location"], "capability": row["required_capability"],
                    "known": float(row["timestamp"]), "deadline": float(row["deadline"]),
                    "service": float(row["time_required"]),
                }
                for row in csv.DictReader(source) if row.get("incident_id")
            }
        roads = {
            (road["from"], road["to"]): float(road["distance"]) / float(road["speed"])
            for road in graph["roads"]
        }
        return {"nodes": {node["id"] for node in graph["nodes"]}, "roads": roads,
                "responders": responders, "incidents": incidents, "blocked_roads": set()}


class LauncherTests(unittest.TestCase):
    def test_run_py_shows_usage_when_scenario_is_omitted(self) -> None:
        result = subprocess.run(
            [sys.executable, str(ROOT / "run.py")], cwd=ROOT, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10, check=False,
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("Usage:", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)

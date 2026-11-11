#!/usr/bin/env python3
"""Build and integration-test the AI emergency-response simulator.

Run from any directory with: python3 test.py
Only Python's standard library and the project's configured C++ toolchain are
required. Each scenario runs in an isolated temporary working directory so its
Documentation outputs never overwrite the checked-in project documentation.
"""

from __future__ import annotations

import json
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
    "city_graph.json",
    "incidents.csv",
    "responders.csv",
    "road_probabilities.json",
    "traffic_probabilities.json",
    "observed_traffic.csv",
    "road_reports.csv",
)


class SimulatorIntegrationTests(unittest.TestCase):
    """Exercise the actual compiled simulator against every supplied dataset."""

    @classmethod
    def setUpClass(cls) -> None:
        build = subprocess.run(
            ["make", "-B"],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=180,
            check=False,
        )
        if build.returncode != 0:
            raise AssertionError(
                f"The project did not build successfully (exit {build.returncode}):\n"
                f"{build.stdout}"
            )
        cls.executable = ROOT / "run"

    def scenario_names(self) -> list[str]:
        return sorted(
            path.name
            for path in SCENARIOS_DIR.iterdir()
            if path.is_dir() and path.name.startswith("Scenario")
        )

    def test_scenario_fixtures_are_complete_and_parseable(self) -> None:
        scenarios = self.scenario_names()
        self.assertTrue(scenarios, "No Scenarios/Scenario* datasets were found")
        for name in scenarios:
            with self.subTest(scenario=name):
                folder = SCENARIOS_DIR / name
                for filename in REQUIRED_INPUTS:
                    self.assertTrue((folder / filename).is_file(), f"Missing {name}/{filename}")
                with (folder / "city_graph.json").open(encoding="utf-8") as source:
                    graph = json.load(source)
                self.assertIsInstance(graph.get("nodes"), list)
                self.assertIsInstance(graph.get("roads"), list)
                self.assertTrue(graph["nodes"], "Scenario graph has no nodes")
                node_ids = {node["id"] for node in graph["nodes"]}
                for road in graph["roads"]:
                    self.assertIn(road["from"], node_ids, "Road starts at an unknown node")
                    self.assertIn(road["to"], node_ids, "Road ends at an unknown node")
                for filename in ("road_probabilities.json", "traffic_probabilities.json"):
                    with (folder / filename).open(encoding="utf-8") as source:
                        self.assertIsInstance(json.load(source), dict)

    def test_each_scenario_runs_and_emits_a_valid_plan_and_statistics(self) -> None:
        scenarios = self.scenario_names()
        self.assertTrue(scenarios, "No Scenarios/Scenario* datasets were found")

        for name in scenarios:
            with self.subTest(scenario=name), tempfile.TemporaryDirectory(prefix="ai-er-test-") as temp:
                scenario_dir = SCENARIOS_DIR / name
                working_dir = Path(temp)
                result = subprocess.run(
                    [str(self.executable), str(scenario_dir)],
                    cwd=working_dir,
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    timeout=90,
                    check=False,
                )
                self.assertEqual(
                    result.returncode,
                    0,
                    f"{name} exited with {result.returncode}.\n"
                    f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}",
                )

                docs = working_dir / "Documentation"
                plan_path = docs / "plan.json"
                stats_path = docs / "statistics.txt"
                self.assertTrue(plan_path.is_file(), f"{name} did not create Documentation/plan.json")
                self.assertTrue(stats_path.is_file(), f"{name} did not create Documentation/statistics.txt")

                with plan_path.open(encoding="utf-8") as source:
                    plan = json.load(source)
                self.assertIsInstance(plan, list, "Plan must be a JSON array")

                with (scenario_dir / "city_graph.json").open(encoding="utf-8") as source:
                    graph = json.load(source)
                nodes = {node["id"] for node in graph["nodes"]}
                incidents = self._read_csv_ids(scenario_dir / "incidents.csv", "incident_id")
                responders = self._read_responders(scenario_dir / "responders.csv")
                incident_locations = self._read_incident_locations(scenario_dir / "incidents.csv")
                incident_deadlines = self._read_incident_deadlines(scenario_dir / "incidents.csv")
                incident_timestamps = self._read_incident_timestamps(scenario_dir / "incidents.csv")
                directed_edges = {(road["from"], road["to"]) for road in graph["roads"]}
                dispatched_incidents: set[str] = set()
                dispatches_by_responder_and_time: set[tuple[str, float]] = set()

                for index, assignment in enumerate(plan):
                    with self.subTest(scenario=name, assignment=index):
                        self.assertIsInstance(assignment, dict)
                        self.assertTrue({"responder_id", "incident_id", "path", "time"}.issubset(assignment))
                        responder_id = assignment["responder_id"]
                        incident_id = assignment["incident_id"]
                        self.assertIn(responder_id, responders, "Plan references an unknown responder")
                        self.assertIn(incident_id, incidents, "Plan references an unknown incident")
                        self.assertNotIn(incident_id, dispatched_incidents, "An incident was dispatched more than once")
                        dispatched_incidents.add(incident_id)

                        path = assignment["path"]
                        self.assertIsInstance(path, list)
                        self.assertTrue(path, "Assigned route must contain at least one node")
                        self.assertTrue(all(node in nodes for node in path), "Route references an unknown node")
                        self.assertEqual(path[-1], incident_locations[incident_id], "Route does not end at incident location")
                        for source, target in zip(path, path[1:]):
                            self.assertIn((source, target), directed_edges, f"Route uses nonexistent edge {source}->{target}")

                        dispatch_time = assignment["time"]
                        self.assertIsInstance(dispatch_time, (int, float))
                        self.assertGreaterEqual(dispatch_time, incident_timestamps[incident_id])
                        self.assertLessEqual(dispatch_time, incident_deadlines[incident_id])
                        dispatch_key = (responder_id, float(dispatch_time))
                        self.assertNotIn(
                            dispatch_key,
                            dispatches_by_responder_and_time,
                            "A responder was assigned to multiple incidents at the same simulation time",
                        )
                        dispatches_by_responder_and_time.add(dispatch_key)

                self.assertLessEqual(len(plan), len(incidents), "Plan contains more dispatches than input incidents")
                statistics = stats_path.read_text(encoding="utf-8")
                self.assertIn("System statistics:", statistics)
                dispatch_count = re.search(r"\* (\d+) incidents handled", statistics)
                self.assertIsNotNone(dispatch_count, "Statistics omit the handled incident count")
                self.assertEqual(int(dispatch_count.group(1)), len(plan))
                self.assertRegex(statistics, r"\* \d+ responders used")
                self.assertRegex(statistics, r"\* Average response time: [\d.eE+-]+s")
                self.assertRegex(statistics, r"\* Deadlines met: \d+%")

    @staticmethod
    def _read_csv_ids(path: Path, column: str) -> set[str]:
        import csv

        with path.open(newline="", encoding="utf-8") as source:
            return {row[column] for row in csv.DictReader(source) if row.get(column)}

    @staticmethod
    def _read_responders(path: Path) -> dict[str, dict[str, Any]]:
        import csv

        with path.open(newline="", encoding="utf-8") as source:
            return {row["responder_id"]: row for row in csv.DictReader(source) if row.get("responder_id")}

    @staticmethod
    def _read_incident_locations(path: Path) -> dict[str, str]:
        import csv

        with path.open(newline="", encoding="utf-8") as source:
            return {row["incident_id"]: row["location"] for row in csv.DictReader(source) if row.get("incident_id")}

    @staticmethod
    def _read_incident_deadlines(path: Path) -> dict[str, float]:
        import csv

        with path.open(newline="", encoding="utf-8") as source:
            return {row["incident_id"]: float(row["deadline"]) for row in csv.DictReader(source) if row.get("incident_id")}

    @staticmethod
    def _read_incident_timestamps(path: Path) -> dict[str, float]:
        import csv

        with path.open(newline="", encoding="utf-8") as source:
            return {row["incident_id"]: float(row["timestamp"]) for row in csv.DictReader(source) if row.get("incident_id")}


class LauncherTests(unittest.TestCase):
    def test_run_py_shows_usage_when_scenario_is_omitted(self) -> None:
        result = subprocess.run(
            [sys.executable, str(ROOT / "run.py")],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=10,
            check=False,
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("Usage:", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)

#!/usr/bin/env python3
"""Simple integration tests for the AI emergency-response simulator.

These tests are intentionally easy to read and focus on the behaviour that
matters most for the project rubric:
- the app builds successfully
- each scenario can run without crashing
- the simulator writes a plan and summary statistics
- the generated plan matches the expected schema
- the summary reports meaningful numbers
"""

from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SCENARIOS_DIR = ROOT / "Scenarios"
REQUIRED_FILES = (
    "city_graph.json",
    "incidents.csv",
    "responders.csv",
    "road_probabilities.json",
    "traffic_probabilities.json",
    "observed_traffic.csv",
    "road_reports.csv",
)
EXPECTED_PLAN_KEYS = {"responder_id", "incident_id", "path", "time"}


class SimpleIntegrationTests(unittest.TestCase):
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
            raise AssertionError(f"Build failed (exit {build.returncode}):\n{build.stdout}")
        cls.executable = ROOT / "run"

    def test_all_scenarios_have_required_input_files(self) -> None:
        scenario_dirs = sorted(path for path in SCENARIOS_DIR.iterdir() if path.is_dir() and path.name.startswith("Scenario"))
        self.assertTrue(scenario_dirs, "No Scenario directories were found in Scenarios/")

        for scenario_dir in scenario_dirs:
            with self.subTest(scenario=scenario_dir.name):
                for filename in REQUIRED_FILES:
                    self.assertTrue(
                        (scenario_dir / filename).is_file(),
                        f"Missing required input: {scenario_dir.name}/{filename}",
                    )

    def test_each_scenario_runs_and_creates_outputs(self) -> None:
        scenario_dirs = sorted(path for path in SCENARIOS_DIR.iterdir() if path.is_dir() and path.name.startswith("Scenario"))

        for scenario_dir in scenario_dirs:
            with self.subTest(scenario=scenario_dir.name), tempfile.TemporaryDirectory(prefix="simple-er-") as temp_dir:
                result = subprocess.run(
                    [str(self.executable), str(scenario_dir)],
                    cwd=temp_dir,
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    timeout=90,
                    check=False,
                )

                self.assertEqual(
                    result.returncode,
                    0,
                    f"{scenario_dir.name} failed to run.\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}",
                )

                docs_dir = Path(temp_dir) / "Documentation"
                plan_path = docs_dir / "plan.json"
                stats_path = docs_dir / "statistics.txt"

                self.assertTrue(plan_path.is_file(), f"{scenario_dir.name} did not create plan.json")
                self.assertTrue(stats_path.is_file(), f"{scenario_dir.name} did not create statistics.txt")

                plan = json.loads(plan_path.read_text(encoding="utf-8"))
                self.assertIsInstance(plan, list, "plan.json should contain a list of assignments")
                self.assertGreater(len(plan), 0, f"{scenario_dir.name} produced an empty plan")

                for item in plan:
                    self.assertEqual(set(item.keys()), EXPECTED_PLAN_KEYS, "Assignment shape is wrong")
                    self.assertTrue(item["path"], "Every assignment needs a path")
                    self.assertIsInstance(item["time"], (int, float), "Dispatch time must be a number")

    def test_statistics_report_meaningful_results(self) -> None:
        for scenario_dir in sorted(path for path in SCENARIOS_DIR.iterdir() if path.is_dir() and path.name.startswith("Scenario")):
            with self.subTest(scenario=scenario_dir.name), tempfile.TemporaryDirectory(prefix="simple-er-stats-") as temp_dir:
                result = subprocess.run(
                    [str(self.executable), str(scenario_dir)],
                    cwd=temp_dir,
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    timeout=90,
                    check=False,
                )
                self.assertEqual(result.returncode, 0, f"{scenario_dir.name} crashed: {result.stderr}")

                stats_text = (Path(temp_dir) / "Documentation" / "statistics.txt").read_text(encoding="utf-8")
                self.assertIn("System statistics:", stats_text)
                self.assertRegex(stats_text, r"\* \d+ incidents handled")
                self.assertRegex(stats_text, r"\* \d+ responders used")
                self.assertRegex(stats_text, r"\* Average response time: [\d.eE+-]+s")
                self.assertRegex(stats_text, r"\* Deadlines met: \d+%")


if __name__ == "__main__":
    unittest.main(verbosity=2)

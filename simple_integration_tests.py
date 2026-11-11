#!/usr/bin/env python3
"""Integration tests that check whether generated dispatch plans are valid.

Run with: python3 simple_integration_tests.py
The tests build and run the real C++ program against every supplied scenario.
"""

from __future__ import annotations

import csv
import json
import math
import subprocess
import tempfile
import unittest
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent
SCENARIOS_DIR = ROOT / "Scenarios"
PLAN_KEYS = {"responder_id", "incident_id", "path", "time"}


def read_scenario(scenario_dir: Path) -> dict[str, Any]:
    """Read the scenario facts needed to check a generated plan."""
    graph = json.loads((scenario_dir / "city_graph.json").read_text(encoding="utf-8"))
    nodes = {node["id"] for node in graph["nodes"]}

    # The simulator's route costs change with traffic and road reports.
    # These base costs let us check that a route is made of real roads; we do
    # not use them to claim that a traffic-dependent deadline is guaranteed.
    roads = {
        (road["from"], road["to"]): float(road["distance"]) / float(road["speed"])
        for road in graph["roads"]
    }

    with (scenario_dir / "responders.csv").open(newline="", encoding="utf-8") as source:
        responders = {}
        for row in csv.DictReader(source):
            responders[row["responder_id"]] = {
                "location": row["start_location"],
                "capabilities": set(row["capabilities"].split(";")),
                "available_at": float(row["time_until_available"])
                if row["availability"] == "Busy" else 0.0,
            }

    with (scenario_dir / "incidents.csv").open(newline="", encoding="utf-8") as source:
        incidents = {
            row["incident_id"]: {
                "location": row["location"],
                "capability": row["required_capability"],
                "known_at": float(row["timestamp"]),
                "deadline": float(row["deadline"]),
                "service_time": float(row["time_required"]),
            }
            for row in csv.DictReader(source)
        }

    return {"nodes": nodes, "roads": roads, "responders": responders, "incidents": incidents}


def plan_errors(plan: Any, scenario: dict[str, Any]) -> list[str]:
    """Return plain-English problems found in a dispatch plan."""
    if not isinstance(plan, list):
        return ["The plan must be a JSON list."]

    errors: list[str] = []
    responders = scenario["responders"]
    incidents = scenario["incidents"]
    seen_incidents: set[str] = set()
    responder_locations = {key: value["location"] for key, value in responders.items()}
    # Keep track of when each responder is busy so we can detect conflicting jobs.
    responder_busy_until = {key: value["available_at"] for key, value in responders.items()}

    for number, item in enumerate(plan, start=1):
        label = f"Assignment {number}"
        if not isinstance(item, dict) or set(item) != PLAN_KEYS:
            errors.append(f"{label} must contain exactly: responder_id, incident_id, path, time.")
            continue

        responder_id = item["responder_id"]
        incident_id = item["incident_id"]
        path = item["path"]
        dispatch_time = item["time"]

        if responder_id not in responders:
            errors.append(f"{label} refers to unknown responder {responder_id!r}.")
            continue
        if incident_id not in incidents:
            errors.append(f"{label} refers to unknown incident {incident_id!r}.")
            continue
        if incident_id in seen_incidents:
            errors.append(f"Incident {incident_id!r} is assigned more than once.")
        seen_incidents.add(incident_id)

        responder = responders[responder_id]
        incident = incidents[incident_id]
        if incident["capability"] not in responder["capabilities"]:
            errors.append(f"Responder {responder_id!r} lacks the capability for {incident_id!r}.")
        if not isinstance(dispatch_time, (int, float)) or isinstance(dispatch_time, bool) or not math.isfinite(dispatch_time) or dispatch_time < 0:
            errors.append(f"{label} has an invalid dispatch time.")
            continue
        if dispatch_time < incident["known_at"]:
            errors.append(f"{label} starts before incident {incident_id!r} is known.")
        if dispatch_time < responder_busy_until[responder_id]:
            errors.append(f"Responder {responder_id!r} has overlapping or too-early assignments.")

        if not isinstance(path, list) or not path:
            errors.append(f"{label} must have a non-empty path.")
            continue
        if path[0] != responder_locations[responder_id]:
            errors.append(f"{label} starts at the wrong responder location.")
        if path[-1] != incident["location"]:
            errors.append(f"{label} does not end at the incident location.")
        if any(node not in scenario["nodes"] for node in path):
            errors.append(f"{label} contains a node that is not in the city graph.")
            continue

        route_edges = list(zip(path, path[1:]))
        if any(edge not in scenario["roads"] for edge in route_edges):
            errors.append(f"{label} uses a road that does not exist in the city graph.")
            continue

        # Update the responder's location and next available time for later jobs.
        base_travel_time = sum(scenario["roads"][edge] for edge in route_edges)
        responder_locations[responder_id] = incident["location"]
        responder_busy_until[responder_id] = dispatch_time + base_travel_time + incident["service_time"]

    return errors


class SimpleIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        build = subprocess.run(
            ["make", "-B"], cwd=ROOT, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            timeout=180, check=False,
        )
        if build.returncode != 0:
            raise AssertionError(f"Build failed (exit {build.returncode}):\n{build.stdout}")
        cls.executable = ROOT / "run"

    def test_generated_plans_are_valid_for_every_scenario(self) -> None:
        scenario_dirs = sorted(
            path for path in SCENARIOS_DIR.iterdir()
            if path.is_dir() and path.name.startswith("Scenario")
        )
        self.assertTrue(scenario_dirs, "No Scenario folders were found.")

        for scenario_dir in scenario_dirs:
            with self.subTest(scenario=scenario_dir.name), tempfile.TemporaryDirectory(prefix="simple-er-") as temp_dir:
                run = subprocess.run(
                    [str(self.executable), str(scenario_dir)], cwd=temp_dir,
                    text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                    timeout=90, check=False,
                )
                self.assertEqual(
                    run.returncode, 0,
                    f"Simulator failed for {scenario_dir.name}.\n{run.stdout}\n{run.stderr}",
                )

                plan_path = Path(temp_dir) / "Documentation" / "plan.json"
                self.assertTrue(plan_path.is_file(), f"{scenario_dir.name} did not write plan.json")
                plan = json.loads(plan_path.read_text(encoding="utf-8"))
                problems = plan_errors(plan, read_scenario(scenario_dir))
                self.assertEqual(problems, [], f"Invalid plan for {scenario_dir.name}:\n- " + "\n- ".join(problems))


if __name__ == "__main__":
    unittest.main(verbosity=2)

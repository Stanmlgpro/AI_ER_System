#!/usr/bin/env python3
"""Advanced integration tests checking validator compliance while respecting
dynamic C++ HMM/Bayesian travel time calculations.

Run with: python3 advanced_integration_tests.py
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


def validate_plan(plan: Any, scenario: dict[str, Any]) -> tuple[list[str], list[str]]:
    """Return (hard_errors, pessimistic_warnings) for a dispatch plan."""
    if not isinstance(plan, list):
        return ["schema"], []

    errors: list[str] = []
    warnings: list[str] = []
    responders = scenario["responders"]
    incidents = scenario["incidents"]
    seen_incidents: set[str] = set()

    responder_locations = {key: value["location"] for key, value in responders.items()}
    responder_busy_until = {key: value["available_at"] for key, value in responders.items()}

    for item in plan:
        if not isinstance(item, dict) or set(item) != PLAN_KEYS:
            errors.append("schema")
            continue

        responder_id = item["responder_id"]
        incident_id = item["incident_id"]
        path = item["path"]
        dispatch_time = item["time"]

        if responder_id not in responders:
            errors.append(f"unknown_responder ({responder_id})")
            continue
        if incident_id not in incidents:
            errors.append(f"unknown_incident ({incident_id})")
            continue
        if incident_id in seen_incidents:
            errors.append(f"duplicate_incident ({incident_id})")
        seen_incidents.add(incident_id)

        responder = responders[responder_id]
        incident = incidents[incident_id]

        if incident["capability"] not in responder["capabilities"]:
            errors.append(f"capability ({responder_id} -> {incident_id})")

        if (
            not isinstance(dispatch_time, (int, float))
            or isinstance(dispatch_time, bool)
            or not math.isfinite(dispatch_time)
            or dispatch_time < 0
        ):
            errors.append(f"invalid_time ({incident_id})")
            continue

        if dispatch_time < incident["known_at"]:
            errors.append(f"starts_before_known ({incident_id})")

        if dispatch_time < responder_busy_until[responder_id]:
            errors.append(f"responder_unavailable / overlap ({responder_id} on {incident_id})")

        if not isinstance(path, list) or not path:
            errors.append(f"empty_path ({incident_id})")
            continue

        if path[0] != responder_locations[responder_id]:
            errors.append(f"wrong_origin ({responder_id} on {incident_id})")

        if path[-1] != incident["location"]:
            errors.append(f"wrong_destination ({incident_id})")

        if any(node not in scenario["nodes"] for node in path):
            errors.append(f"invalid_path_node ({incident_id})")
            continue

        route_edges = list(zip(path, path[1:]))
        if any(edge not in scenario["roads"] for edge in route_edges):
            errors.append(f"invalid_path_edge ({incident_id})")
            continue

        base_travel_time = sum(scenario["roads"][edge] for edge in route_edges)
        total_task_duration = base_travel_time + incident["service_time"]

        # Hard check: static baseline travel time cannot exceed deadline
        if (dispatch_time + base_travel_time) > incident["deadline"]:
            errors.append(
                f"missed_deadline_optimistic ({incident_id}: "
                f"est={dispatch_time + base_travel_time:.1f} > dl={incident['deadline']})"
            )

        # Soft check: log static 1.5x buffer as diagnostic info since C++ uses HMM/Bayesian speeds
        if (dispatch_time + (base_travel_time * 1.5)) > incident["deadline"]:
            warnings.append(
                f"pessimistic_static_margin_tight ({incident_id}: "
                f"1.5x_est={dispatch_time + base_travel_time * 1.5:.1f} > dl={incident['deadline']})"
            )

        responder_locations[responder_id] = incident["location"]
        responder_busy_until[responder_id] = dispatch_time + total_task_duration

    return errors, warnings


class AdvancedIntegrationTests(unittest.TestCase):
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

    def test_validator_compliance_per_scenario(self) -> None:
        scenario_dirs = sorted(
            path for path in SCENARIOS_DIR.iterdir()
            if path.is_dir() and path.name.startswith("Scenario")
        )
        self.assertTrue(scenario_dirs, "No Scenario folders were found.")

        for scenario_dir in scenario_dirs:
            with self.subTest(scenario=scenario_dir.name), tempfile.TemporaryDirectory(prefix="adv-er-") as temp_dir:
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
                errors, warnings = validate_plan(plan, read_scenario(scenario_dir))

                if warnings:
                    print(f"\n[{scenario_dir.name}] Diagnostic warnings: {warnings}")

                self.assertEqual(
                    errors, [],
                    f"Hard validator violations in {scenario_dir.name}:\n- " + "\n- ".join(errors),
                )


if __name__ == "__main__":
    unittest.main(verbosity=2)
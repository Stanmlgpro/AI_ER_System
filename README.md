# AI Emergency Response Simulation System

A modular, time-stepped C++ simulation system designed to handle emergency response dispatch under dynamic city conditions. It incorporates Bayesian inference for physical road states and Hidden Markov Models (HMM) for real-time traffic volume forecasting, utilizing A* pathfinding to route emergency vehicles efficiently.

## Features

* **Bayesian Road Evaluation:** Dynamically assesses road status (`Open`, `Congested`, `Blocked`) based on weather and field reports.
* **HMM Traffic Inference:** Computes traffic volume state sequences (`Low`, `Medium`, `High`) using historical observations and the HMM Forward Algorithm.
* **Intelligent Dispatch Engine:** Prioritizes incoming emergency incidents by deadline and priority level, matching them with capable responders.
* **Automated Runner:** Python wrapper script to manage compilation (`make`) and scenario execution.

## Requirements

* C++17 compatible compiler (GCC/Clang)
* Python 3

## Project Structure

* C++ source files (`Emulator`, `RoadEvaluator`, `CityGraph`, `Documentor`, etc.)
* `Scenarios/` - Scenario-specific configuration files (`city_graph.json`, `incidents.csv`, `responders.csv`, probability JSONs, and traffic observations)
* `Documentation/` - Generated simulation logs (`plan.json` and `statistics.txt`)
* `run.py` - Python automation script

## Usage

Run the simulation for a specific scenario directory using the Python script:

```bash
python3 run.py Scenarios/Scenario1
```

This command automatically triggers `make` to build the project and runs the compiled binary with the specified scenario path.


Run the tests using the Python script:

```bash
python3 simple_integration_tests.py
```

## Output

Simulation results and dispatch plans are exported directly to the `Documentation/` directory:
* `Documentation/plan.json`: Detailed timeline of responder assignments and routing paths.
* `Documentation/statistics.txt`: Summary performance and execution statistics.
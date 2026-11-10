#include <iostream>
#include <string>
#include "responders.h"
#include "incidents.h"
#include "city_graph.h"
#include "road_evaluator.h"
#include "emulator.h"

int main(int argc, char* argv[]) {
    std::string scenario_dir = argv[1];
    // Ensure trailing slash if not present
    if (!scenario_dir.empty() && scenario_dir.back() != '/') {
        scenario_dir += "/";
    }

    CityGraph city = load_city_graph(scenario_dir + "city_graph.json");
    std::vector<Incident> incidents = load_incidents(scenario_dir + "incidents.csv");
    std::vector<Responder> responders = load_responders(scenario_dir + "responders.csv");

    RoadEvaluator evaluator;
    // Load both probability JSON configs into the evaluator object
    evaluator.load_configurations(scenario_dir + "road_probabilities.json", scenario_dir + "traffic_probabilities.json"
        , scenario_dir + "observed_traffic.csv", scenario_dir + "road_reports.csv");

    Emulator emulator(&city, incidents, responders);

    for (int t = 0; t <= 10; t++) {
        int current_time = t; // Simulated time step
        // Update static road states using weather & reports CSVs
        evaluator.update_city_road_states(city);
        // Update dynamic traffic states using the observations CSV and current timestamp
        evaluator.update_city_traffic_states(city, current_time);

        emulator.run_tick(current_time);
    }

    return 0;
}
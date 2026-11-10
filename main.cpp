#include <iostream>
#include <string>
#include "responders.h"
#include "incidents.h"
#include "city_graph.h"

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0] << " <scenario_directory>\n";
            return 1;
        }

        std::string scenario_dir = argv[1];
        // Ensure trailing slash if not present
        if (!scenario_dir.empty() && scenario_dir.back() != '/') {
            scenario_dir += "/";
        }

        CityGraph city = load_city_graph(scenario_dir + "city_graph.json");
        std::cout << "Successfully loaded " << city.nodes.size() << " nodes." << std::endl;
        std::cout << "Successfully loaded adjacency lists for " << city.adj_list.size() << " starting nodes." << std::endl;

        std::vector<Incident> incidents = load_incidents(scenario_dir + "incidents.csv");
        std::cout << "Successfully loaded " << incidents.size() << " incidents." << std::endl;
        for (const auto& inc : incidents) {
            std::cout << " - Incident " << inc.incident_id << " at " << inc.location 
                    << " (Deadline: " << inc.deadline << ")\n";
        }

        std::vector<Responder> responders = load_responders(scenario_dir + "responders.csv");
        std::cout << "Successfully loaded " << responders.size() << " responders." << std::endl;
        for (const auto& resp : responders) {
            std::cout << " - Responder " << resp.responder_id << " at " << resp.start_location << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
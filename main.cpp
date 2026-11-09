#include <iostream>
#include "responders.h"
#include "incidents.h"
#include "city_graph.h"

int main() {
    try {
        CityGraph city = load_city_graph("Scenarios/Scenario1/city_graph.json");
        std::cout << "Successfully loaded " << city.nodes.size() << " nodes." << std::endl;
        std::cout << "Successfully loaded adjacency lists for " << city.adj_list.size() << " starting nodes." << std::endl;

        std::vector<Incident> incidents = load_incidents("Scenarios/Scenario1/incidents.csv");
        std::cout << "Successfully loaded " << incidents.size() << " incidents." << std::endl;
        for (const auto& inc : incidents) {
            std::cout << " - Incident " << inc.incident_id << " at " << inc.location 
                    << " (Deadline: " << inc.deadline << ")\n";
        }

        std::vector<Responder> responders = load_responders("Scenarios/Scenario1/responders.csv");
        std::cout << "Successfully loaded " << responders.size() << " responders." << std::endl;
        for (const auto& resp : responders) {
            std::cout << " - Responder " << resp.responder_id << " at " << resp.start_location << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
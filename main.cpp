#include <iostream>
#include "responders.h"
#include "incidents.h"
#include "city_graph.h"

int main() {
    try {
        CityGraph city = load_city_graph("Scenarios/Scenario1/city_graph.json");
        std::cout << "Successfully loaded " << city.nodes.size() << " nodes." << std::endl;
        std::cout << "Successfully loaded adjacency lists for " << city.adj_list.size() << " starting nodes." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
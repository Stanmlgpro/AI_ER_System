#include "city_graph.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

CityGraph load_city_graph(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filepath);
    }

    json j;
    file >> j;

    CityGraph graph;

    // 1. Parse nodes
    for (const auto& j_node : j["nodes"]) {
        Node node;
        node.id = j_node["id"].get<std::string>();
        node.name = j_node["name"].get<std::string>();
        node.x = j_node["x"].get<double>();
        node.y = j_node["y"].get<double>();
        node.type = j_node["type"].get<std::string>();

        graph.nodes[node.id] = node;
    }

    // 2. Parse roads and build adjacency list
    for (const auto& j_road : j["roads"]) {
        std::string from = j_road["from"].get<std::string>();
        
        Road road;
        road.to_node = j_road["to"].get<std::string>();
        road.distance = j_road["distance"].get<double>();
        road.road_type = j_road["road_type"].get<std::string>();
        road.speed = j_road["speed"].get<double>();

        // Push outgoing road into the map
        graph.adj_list[from].push_back(road);
    }

    return graph;
}
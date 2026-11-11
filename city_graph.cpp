#include "city_graph.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <cmath>
#include <algorithm>
#include <limits>
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

    for (const auto& j_node : j["nodes"]) {
        Node node;
        node.id = j_node["id"].get<std::string>();
        node.name = j_node["name"].get<std::string>();
        node.x = j_node["x"].get<double>();
        node.y = j_node["y"].get<double>();
        node.type = j_node["type"].get<std::string>();

        graph.nodes[node.id] = node;
    }

    for (const auto& j_road : j["roads"]) {
        std::string from = j_road["from"].get<std::string>();
        
        Road road;
        road.to_node = j_road["to"].get<std::string>();
        road.distance = j_road["distance"].get<double>();
        road.road_type = j_road["road_type"].get<std::string>();
        road.speed = j_road["speed"].get<double>();
        road.base_travel_time = (road.speed > 0) ? (road.distance / (road.speed)) : road.distance;
        road.state = RoadState::OPEN; // Default state

        graph.adj_list[from].push_back(road);
    }

    return graph;
}

double Road::get_travel_time() const {
    double travel_time = base_travel_time;
    if (traffic_volume == "low") {
        travel_time /= 2.0;
    } else if (traffic_volume == "medium") {
        travel_time /= 1.5;
    }
    

    if (state == RoadState::CONGESTED) {
        travel_time *= 1.5;
    } else if (state == RoadState::BLOCKED) {
        travel_time = std::numeric_limits<double>::infinity();
    }

    if (road_type == "main") {
        travel_time *= 0.7; // 10% faster on main roads
    } else if (road_type == "secondary") {
        travel_time *= 1.2; // 10% slower on secondary roads
    }
    return travel_time;
}

static double heuristic(const Node& a, const Node& b, double max_speed) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy) / max_speed;
}

std::vector<Node> A_star(const CityGraph& graph, const std::string& start_node_id, const std::string& goal_node_id) {
    if (graph.nodes.find(start_node_id) == graph.nodes.end() || 
        graph.nodes.find(goal_node_id) == graph.nodes.end()) {
        return {};
    }

    // maximum speed for admissible heuristic
    double max_speed = 1.0;
    for (const auto& [from_id, roads] : graph.adj_list) {
        for (const auto& road : roads) {
            if (road.speed > max_speed) {
                max_speed = road.speed;
            }
        }
    }

    const Node& goal_node = graph.nodes.at(goal_node_id);

    using FringeNode = std::pair<double, std::string>;
    std::priority_queue<FringeNode, std::vector<FringeNode>, std::greater<FringeNode>> fringe;

    std::unordered_map<std::string, double> g_score;
    std::unordered_map<std::string, std::string> came_from;

    for (const auto& [id, node] : graph.nodes) {
        g_score[id] = std::numeric_limits<double>::infinity();
    }

    g_score[start_node_id] = 0.0;
    fringe.push({heuristic(graph.nodes.at(start_node_id), goal_node, max_speed), start_node_id});

    while (!fringe.empty()) {
        auto [current_f, current_id] = fringe.top();
        fringe.pop();

        if (current_id == goal_node_id) {
            std::vector<Node> path;
            std::string curr = goal_node_id;
            while (curr != start_node_id) {
                path.push_back(graph.nodes.at(curr));
                curr = came_from[curr];
            }
            path.push_back(graph.nodes.at(start_node_id));
            std::reverse(path.begin(), path.end());
            return path;
        }

        auto it = graph.adj_list.find(current_id);
        if (it != graph.adj_list.end()) {
            for (const auto& road : it->second) {
                std::string neighbor_id = road.to_node;
                double tentative_g = g_score[current_id] + road.get_travel_time();

                if (tentative_g < g_score[neighbor_id]) {
                    came_from[neighbor_id] = current_id;
                    g_score[neighbor_id] = tentative_g;
                    double f_score = tentative_g + heuristic(graph.nodes.at(neighbor_id), goal_node, max_speed);
                    fringe.push({f_score, neighbor_id});
                }
            }
        }
    }

    return {};
}
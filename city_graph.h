#ifndef CITY_GRAPH_H
#define CITY_GRAPH_H

#include <string>
#include <vector>
#include <unordered_map>
#include "enums.h"

struct Road {
    std::string to_node;
    double distance;
    double speed;
    std::string road_type;
    std::string traffic_volume; // to store traffic volume (Low, Medium, High)
    double base_travel_time;
    RoadState state;
    double get_travel_time() const;
};

struct Node {
    std::string id;
    std::string name;
    std::string type;
    double x;
    double y;
};

struct CityGraph {
    std::unordered_map<std::string, Node> nodes;
    // Adjacency list: node_id -> list of outgoing roads
    std::unordered_map<std::string, std::vector<Road>> adj_list;
};

CityGraph load_city_graph(const std::string& filepath);

std::vector<Node> A_star(const CityGraph& graph, const std::string& start_node_id, const std::string& goal_node_id);

#endif // CITY_GRAPH_H
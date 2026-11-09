#ifndef CITY_GRAPH_H
#define CITY_GRAPH_H

#include <string>
#include <vector>
#include <unordered_map>

struct Road {
    std::string to_node;
    double distance;
    double speed;
    std::string road_type;
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

#endif // CITY_GRAPH_H
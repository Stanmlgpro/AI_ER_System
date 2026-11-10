#ifndef ROAD_EVALUATOR_H
#define ROAD_EVALUATOR_H

#include <string>
#include <vector>
#include <unordered_map>
#include "city_graph.h"
#include "enums.h"
#include "nlohmann/json.hpp"

struct RoadReport {
    std::string from_node;
    std::string to_node;
    WeatherCondition weather;
    RoadStateSources source;
    RoadState reported_status;
};

struct TrafficObservation {
    int observation_nr;
    std::string from_node;
    std::string to_node;
    std::string observation;
};

class RoadEvaluator {
public:
    void load_configurations(const std::string& road_prob_json_path, const std::string& traffic_prob_json_path, const std::string& traffic_csv, const std::string& reports_csv);
    std::vector<RoadReport> load_road_reports(const std::string& csv_path);
    std::vector<TrafficObservation> load_traffic_observations(const std::string& csv_path);

    void update_city_road_states(CityGraph& graph);

    void update_city_traffic_states(CityGraph& graph, int current_time);

    WeatherCondition parse_weather(const std::string& str);
    RoadStateSources parse_source(const std::string& str);
    RoadState parse_road_state(const std::string& str);
    std::string road_state_to_string(RoadState state);
    std::string weather_to_string(WeatherCondition weather);
    std::string source_to_string(RoadStateSources source);

private:
    nlohmann::json road_prob_config_;
    nlohmann::json traffic_prob_config_;
    std::vector<TrafficObservation> all_observations;
    std::vector<RoadReport> reports;

    RoadState evaluate_bayesian_status(
        WeatherCondition weather, 
        const std::vector<std::pair<RoadStateSources, RoadState>>& reports
    );
    
    std::string evaluate_hmm_traffic(const std::vector<std::string>& observations);
};

#endif // ROAD_EVALUATOR_H
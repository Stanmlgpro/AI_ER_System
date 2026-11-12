#include "road_evaluator.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <iostream>

void RoadEvaluator::load_configurations(const std::string& road_prob_json_path, const std::string& traffic_prob_json_path, const std::string& traffic_csv, const std::string& reports_csv) {
    std::ifstream f1(road_prob_json_path);
    if (!f1.is_open()) throw std::runtime_error("Failed to open " + road_prob_json_path);
    f1 >> road_prob_config_;

    std::ifstream f2(traffic_prob_json_path);
    if (!f2.is_open()) throw std::runtime_error("Failed to open " + traffic_prob_json_path);
    f2 >> traffic_prob_config_;

    all_observations = load_traffic_observations(traffic_csv);
    reports = load_road_reports(reports_csv);
}

WeatherCondition RoadEvaluator::parse_weather(const std::string& str) {
    if (str == "Clear") return WeatherCondition::CLEAR;
    if (str == "Rain") return WeatherCondition::RAIN;
    if (str == "Storm") return WeatherCondition::STORM;
    throw std::runtime_error("Unknown weather condition: " + str);
}

RoadStateSources RoadEvaluator::parse_source(const std::string& str) {
    if (str == "Citizen" || str == "Citizens") return RoadStateSources::CITIZENS;
    if (str == "Police") return RoadStateSources::POLICE;
    if (str == "Sensor" || str == "Sensors") return RoadStateSources::SENSORS;
    throw std::runtime_error("Unknown road state source: " + str);
}

RoadState RoadEvaluator::parse_road_state(const std::string& str) {
    if (str == "Open") return RoadState::OPEN;
    if (str == "Congested") return RoadState::CONGESTED;
    if (str == "Blocked") return RoadState::BLOCKED;
    throw std::runtime_error("Unknown road state: " + str);
}

std::string RoadEvaluator::road_state_to_string(RoadState state) {
    switch (state) {
        case RoadState::OPEN: return "Open";
        case RoadState::CONGESTED: return "Congested";
        case RoadState::BLOCKED: return "Blocked";
    }
    return "Open";
}

std::string RoadEvaluator::weather_to_string(WeatherCondition weather) {
    switch (weather) {
        case WeatherCondition::CLEAR: return "Clear";
        case WeatherCondition::RAIN: return "Rain";
        case WeatherCondition::STORM: return "Storm";
    }
    return "Clear";
}

std::string RoadEvaluator::source_to_string(RoadStateSources source) {
    switch (source) {
        case RoadStateSources::CITIZENS: return "citizen_report";
        case RoadStateSources::POLICE: return "police_report";
        case RoadStateSources::SENSORS: return "sensor_report";
    }
    return "citizen_report";
}

std::vector<RoadReport> RoadEvaluator::load_road_reports(const std::string& csv_path) {
    std::ifstream file(csv_path);
    if (!file.is_open()) throw std::runtime_error("Failed to open " + csv_path);

    std::vector<RoadReport> reports;
    std::string line;
    std::getline(file, line); // Skip header

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string from, to, weather_str, source_str, status_str;
        if (std::getline(ss, from, ',') &&
            std::getline(ss, to, ',') &&
            std::getline(ss, weather_str, ',') &&
            std::getline(ss, source_str, ',') &&
            std::getline(ss, status_str, ',')) {
            reports.push_back({
                from, 
                to, 
                parse_weather(weather_str), 
                parse_source(source_str), 
                parse_road_state(status_str)
            });
        }
    }
    return reports;
}

std::vector<TrafficObservation> RoadEvaluator::load_traffic_observations(const std::string& csv_path) {
    std::ifstream file(csv_path);
    if (!file.is_open()) throw std::runtime_error("Failed to open " + csv_path);

    std::vector<TrafficObservation> obs_list;
    std::string line;
    std::getline(file, line); // Skip header

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string nr_str, from, to, obs;
        if (std::getline(ss, nr_str, ',') &&
            std::getline(ss, from, ',') &&
            std::getline(ss, to, ',') &&
            std::getline(ss, obs, ',')) {
            if (!nr_str.empty()) {
                obs_list.push_back({std::stoi(nr_str), from, to, obs});
            }
        }
    }
    return obs_list;
}

RoadState RoadEvaluator::evaluate_bayesian_status(
    WeatherCondition weather, 
    const std::vector<std::pair<RoadStateSources, RoadState>>& reports
) {
    std::vector<std::string> states = {"Open", "Congested", "Blocked"};
    std::unordered_map<std::string, double> posteriors;

    std::string weather_key = weather_to_string(weather);

    // Prior P(Status | Weather)
    for (const auto& state : states) {  
        posteriors[state] = road_prob_config_["road_status_given_weather"][weather_key][state].get<double>();
    }

    // Likelihood update from reports: P(Report | Status)
    for (const auto& [source, reported_status] : reports) {
        std::string source_key = source_to_string(source);
        std::string rep_status_str = road_state_to_string(reported_status);

        for (const auto& state : states) {
            double p_report_given_state = road_prob_config_[source_key][state][rep_status_str].get<double>();
            posteriors[state] *= p_report_given_state;
        }
    }

    // Normalize posterior probabilities
    double total = 0.0;
    for (const auto& [state, prob] : posteriors) {
        total += prob;
    }

    std::string best_state = "Open";
    double max_prob = -1.0;
    for (const auto& [state, prob] : posteriors) {
        double normalized = (total > 0.0) ? (prob / total) : (1.0 / 3.0);
        if (normalized > max_prob) {
            max_prob = normalized;
            best_state = state;
        }
    }

    return parse_road_state(best_state);
}

std::string RoadEvaluator::evaluate_hmm_traffic(const std::vector<std::string>& observations) {
    if (observations.empty()) return "Low";

    std::vector<std::string> states = {"Low", "Medium", "High"};
    std::unordered_map<std::string, double> beliefs;

    for (const auto& state : states) {
        beliefs[state] = traffic_prob_config_["initial"][state].get<double>() *
                         traffic_prob_config_["emission"][state][observations[0]].get<double>();
    }

    for (size_t t = 1; t < observations.size(); ++t) {
        std::unordered_map<std::string, double> next_beliefs;
        std::string obs = observations[t];

        for (const auto& next_state : states) {
            double sum = 0.0;
            // Prediction Step (Transition)
            for (const auto& prev_state : states) {
                double trans_prob = traffic_prob_config_["transition"][prev_state][next_state].get<double>();
                sum += beliefs[prev_state] * trans_prob;
            }
            // Update Step (Emission / Sensor Noise Filtering)
            double emission_prob = traffic_prob_config_["emission"][next_state][obs].get<double>();
            next_beliefs[next_state] = sum * emission_prob;
        }
        beliefs = next_beliefs;
    }

    std::string best_state = "Low";
    double max_prob = -1.0;
    for (const auto& [state, prob] : beliefs) {
        if (prob > max_prob) {
            max_prob = prob;
            best_state = state;
        }
    }
    return best_state;
}

void RoadEvaluator::update_city_road_states(CityGraph& graph) {
    
    std::unordered_map<std::string, std::vector<std::pair<RoadStateSources, RoadState>>> road_reports_map;
    std::unordered_map<std::string, WeatherCondition> road_weather_map;

    for (const auto& rep : reports) {
        std::string key = rep.from_node + "->" + rep.to_node;
        road_reports_map[key].push_back({rep.source, rep.reported_status});
        road_weather_map[key] = rep.weather;
    }

    for (auto& [from_id, roads] : graph.adj_list) {
        for (auto& road : roads) {
            std::string key = from_id + "->" + road.to_node;
            if (road_reports_map.find(key) != road_reports_map.end()) {
                WeatherCondition weather = road_weather_map[key];
                road.state = evaluate_bayesian_status(weather, road_reports_map[key]);
            } else {
                road.state = RoadState::OPEN; 
            }
        }
    }
}

void RoadEvaluator::update_city_traffic_states(CityGraph& graph, int current_time) {
    // 1. Load all observed traffic records from the CSV

    // 2. Group observations by road segment (from_node -> to_node)
    std::unordered_map<std::string, std::vector<TrafficObservation>> road_obs_map;
    for (const auto& obs : all_observations) {
        if (obs.observation_nr <= current_time) { // Only consider data up to the current timestamp
            std::string key = obs.from_node + "->" + obs.to_node;
            road_obs_map[key].push_back(obs);
        }
    }

    // 3. Iterate through every road in the CityGraph and evaluate its HMM traffic state
    for (auto& [from_id, roads] : graph.adj_list) {
        for (auto& road : roads) {
            std::string key = from_id + "->" + road.to_node;
            
            std::vector<std::string> sequential_obs;
            if (road_obs_map.find(key) != road_obs_map.end()) {
                // Sort observations chronologically by observation_nr
                auto& obs_list = road_obs_map[key];
                std::sort(obs_list.begin(), obs_list.end(), [](const auto& a, const auto& b) {
                    return a.observation_nr < b.observation_nr;
                });

                for (const auto& obs : obs_list) {
                    sequential_obs.push_back(obs.observation);
                }
            }

            // Run the HMM Forward Algorithm for this specific road up to current_time
            std::string traffic_state = evaluate_hmm_traffic(sequential_obs);
            
            // Store or map the resulting traffic state to the road object
            road.traffic_volume = traffic_state; // (Make sure to add std::string traffic_volume to your Road struct)
        }
    }
}
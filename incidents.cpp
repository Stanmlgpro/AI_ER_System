#include "incidents.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

std::vector<Incident> load_incidents(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open incidents file: " + filepath);
    }

    std::vector<Incident> incidents;
    std::string line;

    // Skip the header line
    if (!std::getline(file, line)) {
        return incidents;
    }

    // Read line by line
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string segment;
        Incident inc;

        // Parse CSV fields in order: incident_id, location, priority, deadline, required_capability, timestamp, time_required
        if (std::getline(ss, inc.incident_id, ',') &&
            std::getline(ss, inc.location, ',') &&
            std::getline(ss, segment, ',')) {
            // Convert the priority string to the enum value
            if (segment == "Critical") {
                inc.priority = IncidentPriorityLevel::CRITICAL;
            } else if (segment == "High") {
                inc.priority = IncidentPriorityLevel::HIGH;
            } else if (segment == "Medium") {
                inc.priority = IncidentPriorityLevel::MEDIUM;
            } else if (segment == "Low") {
                inc.priority = IncidentPriorityLevel::LOW;
            }

            if (std::getline(ss, segment, ',')) {
                inc.deadline = std::stod(segment);
            }

            if (std::getline(ss, inc.required_capability, ',') &&
                std::getline(ss, segment, ',')) {
                inc.timestamp = std::stod(segment);

                if (std::getline(ss, segment, ',')) {
                    inc.time_required = std::stod(segment);
                    incidents.push_back(inc);
                }
            }
        }
    }

    return incidents;
}
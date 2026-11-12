#include "responders.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

// Helper to split capability strings separated by semicolons
static std::vector<std::string> split_capabilities(const std::string& str) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, ';')) {
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

// Helper to map string to ResponderType enum
static ResponderType parse_responder_type(const std::string& str) {
    if (str == "FireTruck") return ResponderType::FIRE;
    if (str == "Ambulance") return ResponderType::AMBULANCE;
    if (str == "PoliceUnit") return ResponderType::POLICE;
    throw std::runtime_error("Unknown responder type: " + str);
}

// Helper to map string to AvailabilityStatus enum
static AvailabilityStatus parse_availability(const std::string& str) {
    if (str == "Available") return AvailabilityStatus::AVAILABLE;
    if (str == "Busy") return AvailabilityStatus::BUSY;
    throw std::runtime_error("Unknown availability status: " + str);
}

std::vector<Responder> load_responders(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open responders file: " + filepath);
    }

    std::vector<Responder> responders;
    std::string line;

    // Skip header line
    if (!std::getline(file, line)) {
        return responders;
    }

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string segment;
        Responder resp;
        std::string type_str, avail_str;

        // Parse fields in order: responder_id, type, start_location, capabilities, availability, time_until_available
        if (std::getline(ss, resp.responder_id, ',') &&
            std::getline(ss, type_str, ',') &&
            std::getline(ss, resp.start_location, ',') &&
            std::getline(ss, segment, ',')) {
            
            resp.type = parse_responder_type(type_str);
            resp.capabilities = split_capabilities(segment);

            if (std::getline(ss, avail_str, ',') &&
                std::getline(ss, segment, ',')) {
                resp.availability = parse_availability(avail_str);
                resp.time_until_available = std::stod(segment) + 1; // Adjust for tick-zero decrement
                responders.push_back(resp);
            }
        }
    }

    return responders;
}
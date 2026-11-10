#include "documentor.h"
#include <fstream>
#include <filesystem>
#include <stdexcept>

#include "documentor.h"
#include <fstream>
#include <filesystem>
#include "nlohmann/json.hpp"

void Documentor::document(const tick_document& doc) {
    // Ensure the Documentation directory exists
    std::filesystem::create_directories("Documentation");

    // Build JSON array matching your required format
    nlohmann::json j_array = nlohmann::json::array();
    for (const auto& assignment : doc.assignments) {
        nlohmann::json j_item;
        j_item["responder_id"] = assignment.responder_id;
        j_item["incident_id"] = assignment.incident_id;
        j_item["path"] = assignment.path;
        j_item["time"] = assignment.time;
        j_array.push_back(j_item);
    }

    // Write formatted JSON to Documentation/plan.json
    std::ofstream json_file("Documentation/plan.json");
    if (json_file.is_open()) {
        json_file << j_array.dump(4);
    }

    // Write companion statistics file
    std::ofstream txt_file("Documentation/statistics.txt");
    if (txt_file.is_open()) {
        txt_file << "--- Simulation Statistics ---\n";
        txt_file << "Status: Completed\n";
        txt_file << "Total Dispatches: " << doc.assignments.size() << "\n";
    }
}
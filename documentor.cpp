#include "documentor.h"
#include <fstream>
#include <filesystem>
#include "nlohmann/json.hpp"
#include <iostream>

void Documentor::document(const tick_document& doc, int current_time) {
    // Just buffer the incoming assignments and count total dispatches
    for (const auto& assignment : doc.assignments) {
        all_assignments_.push_back(assignment);
        if (std::find(responder_ids.begin(), responder_ids.end(), assignment.responder_id) == responder_ids.end()) {
            responder_ids.push_back(assignment.responder_id);
        }
    }
    total_dispatches_ += doc.assignments.size();
    current_time_ = current_time;
}

void Documentor::document_response(double response_time) {
    total_response_time_ += response_time;
}

void Documentor::generate_documentation() {
    // Ensure the Documentation directory exists
    std::filesystem::create_directories("Documentation");

    // 1. Build and write the complete JSON array
    nlohmann::json j_array = nlohmann::json::array();
    for (const auto& assignment : all_assignments_) {
        nlohmann::json j_item;
        j_item["responder_id"] = assignment.responder_id;
        j_item["incident_id"] = assignment.incident_id;
        j_item["path"] = assignment.path;
        j_item["time"] = assignment.time;
        j_array.push_back(j_item);
    }

    std::ofstream json_file("Documentation/plan.json");
    if (json_file.is_open()) {
        json_file << j_array.dump(4);
    }

    // 2. Write the companion statistics file
    std::ofstream txt_file("Documentation/statistics.txt");
    if (txt_file.is_open()) {

        txt_file << "System statistics:\n\n";
        txt_file << "* " << total_dispatches_ << " incidents handled\n";
        txt_file << "* " << responder_ids.size() << " responders used\n";
        txt_file << "* Average response time: " << (all_assignments_.empty() ? 0.0 : total_response_time_ / all_assignments_.size()) << "s\n";
        txt_file << "* Deadlines met: " << (total_num_incidents_ > 0 ? std::to_string(total_dispatches_ * 100 / total_num_incidents_) : "0") << "%\n";
    }
}
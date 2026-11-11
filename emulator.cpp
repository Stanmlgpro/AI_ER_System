#include "emulator.h"
#include <iostream>
#include <algorithm>
#include <limits>
#include "documentor.h"

Emulator::Emulator(Documentor& documentor, CityGraph* city_, const std::vector<Incident>& incidents_, const std::vector<Responder>& responders_)
    : documentor(documentor), city(city_), incidents(incidents_), responders(responders_) {}

int get_priority_weight(IncidentPriorityLevel priority) {
    switch (priority) {
        case IncidentPriorityLevel::CRITICAL: return 0; // Highest priority
        case IncidentPriorityLevel::HIGH:     return 1;
        case IncidentPriorityLevel::MEDIUM:   return 2;
        case IncidentPriorityLevel::LOW:      return 3; // Lowest priority
        default:                              return 4;
    }
}

void Emulator::run_tick(int current_time) {
    // 1. Move incidents whose timestamp has arrived into unresolved_incidents ONCE
    for (auto it = incidents.begin(); it != incidents.end(); ) {
        if (it->timestamp <= current_time) {
            unresolved_incidents.push_back(*it);
            it = incidents.erase(it);
        } else {
            ++it;
        }
    }

    // 2. Sort unresolved incidents by Priority (highest first) and Deadline (earliest first)
    std::sort(unresolved_incidents.begin(), unresolved_incidents.end(), [](const Incident& a, const Incident& b) {
    if (a.deadline != b.deadline) {
        return a.deadline < b.deadline; // Primary: Earlier deadline goes first
    }
    int weight_a = get_priority_weight(a.priority);
    int weight_b = get_priority_weight(b.priority);
    return weight_a < weight_b; // Tie-breaker: Higher priority (lower weight) goes first
    });

    // 3. Decrement busy responders
    for (auto& responder : responders) {
        if (responder.availability == AvailabilityStatus::BUSY) {
            if (responder.time_until_available > 0) {
                responder.time_until_available -= 1;
                if (responder.time_until_available <= 0) {
                    responder.availability = AvailabilityStatus::AVAILABLE;
                }
            }
        }
    }

    // 4. Build available_now list for this tick
    std::vector<Responder> available_now;
    for (auto& responder : responders) {
        if (responder.availability == AvailabilityStatus::AVAILABLE) {
            available_now.push_back(responder);
        }
    }

    // 5. Process unresolved incidents
    for (auto it = unresolved_incidents.begin(); it != unresolved_incidents.end();) {
        if (current_time > it->deadline) {
            it = unresolved_incidents.erase(it); // Remove from unresolved since it's too late
            continue;
        }
        Incident& incident = *it;
        bool assigned = false;

        assigned = find_responder_for_incident(incident, available_now, current_time);
                  
        if (assigned) {
            it = unresolved_incidents.erase(it); // Successfully handled and removed from queue
        } else {
            ++it; // Keep in unresolved to try again next tick
        }
    }
}

bool Emulator::find_responder_for_incident(Incident& incident, std::vector<Responder>& available_responders, int current_time) {
    std::vector<Responder*> suitable_responders;
    for (auto& responder : available_responders) {
        if (std::find(responder.capabilities.begin(), responder.capabilities.end(), incident.required_capability) != responder.capabilities.end()) {
            suitable_responders.push_back(&responder);
        }
    }

    Responder* best_responder = nullptr;
    double best_time = std::numeric_limits<double>::infinity();
    std::vector<std::string> best_path_ids;

    for (auto* responder : suitable_responders) {
        std::vector<Node> path = A_star(*city, responder->start_location, incident.location);
        if (!path.empty()) {
            double travel_time = 0.0;
            std::vector<std::string> current_path_ids;
            for (size_t i = 0; i < path.size() - 1; ++i) {
                const auto& from_node = path[i];
                const auto& to_node = path[i + 1];
                current_path_ids.push_back(from_node.id);

                const auto& roads = city->adj_list.at(from_node.id);
                auto it = std::find_if(roads.begin(), roads.end(), [&](const Road& road) { return road.to_node == to_node.id; });
                if (it != roads.end()) {
                    travel_time += it->get_travel_time();
                } else {
                    travel_time = std::numeric_limits<double>::infinity();
                    break;
                }
            }
            if (!path.empty()) {
                current_path_ids.push_back(path.back().id);
            }

            if (travel_time < best_time) {
                best_time = travel_time;
                best_responder = responder;
                best_path_ids = current_path_ids;
            }
        }
    }

    if (best_responder) {
        int working_time = best_time + incident.time_required; 
        if (current_time + best_time > incident.deadline) {
            return false;
        } // If the responder cannot meet the deadline, do not assign
        for (auto& responder : responders) {
            if (responder.responder_id == best_responder->responder_id) {
                responder.availability = AvailabilityStatus::BUSY;
                responder.time_until_available = working_time; 
                responder.start_location = incident.location; // Update responder location
                break;
            }
        }
        documentor.document_response(best_time);

        // Construct the document matching the required plan.json schema
        Assignment assignment;
        assignment.responder_id = best_responder->responder_id;
        assignment.incident_id = incident.incident_id;
        assignment.path = best_path_ids;
        assignment.time = current_time;

        tick_document doc;
        doc.assignments.push_back(assignment);

        documentor.document(doc, current_time);

        // Remove the assigned responder from the available list for this tick to prevent double-assignment
        available_responders.erase(
            std::remove_if(available_responders.begin(), available_responders.end(),
                           [best_responder](const Responder& r) { return r.responder_id == best_responder->responder_id; }),
            available_responders.end()
        );

        return true; // Responder assigned
    }

    return false; // No suitable responder found
}

bool Emulator::check_end_conditions() const {
    bool all_free = true;

    for (const auto& responder : responders) {
        if (responder.availability != AvailabilityStatus::AVAILABLE) {
            all_free = false;
            break;
        }
    }

    // Simulation is done when no future incidents are left, no unresolved incidents remain, and all responders are free
    if (all_free && incidents.empty() && unresolved_incidents.empty()) {
        return true;
    }
    return false;
}

Emulator::~Emulator() = default;
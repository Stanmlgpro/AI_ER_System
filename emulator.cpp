#include "emulator.h"
#include <iostream>
#include <algorithm>
#include <limits>
#include "documentor.h"

Emulator::Emulator(CityGraph* city_, const std::vector<Incident>& incidents_, const std::vector<Responder>& responders_)
    : city(city_), incidents(incidents_), responders(responders_) {}

void Emulator::run_tick(int current_time) {
    for (const auto& incident : incidents) {
        if (incident.timestamp == current_time) {
            unresolved_incidents.push_back(incident);
        }
    }

    std::vector<Responder> available_now;
    for (auto& responder : responders) {
        if (responder.availability == AvailabilityStatus::AVAILABLE) {
            available_now.push_back(responder);
        } else if (responder.time_until_available > 0) {
            responder.time_until_available -= 1; // Decrement time until available
            if (responder.time_until_available <= 0) {
                responder.availability = AvailabilityStatus::AVAILABLE;
                available_now.push_back(responder);
            }
        }
    }

    for (auto it = unresolved_incidents.begin(); it != unresolved_incidents.end();) {
        Incident& incident = *it;
        bool assigned = false;

        assigned = find_responder_for_incident(incident, available_now);

        if (assigned) {
            it = unresolved_incidents.erase(it); // Remove the incident from unresolved list
        } else {
            ++it; // Move to the next incident
        }
    }
}

bool Emulator::find_responder_for_incident(Incident& incident, std::vector<Responder>& available_responders) {
    std::vector<Responder*> suitable_responders;
    for (auto& responder : available_responders) {
        if (std::find(responder.capabilities.begin(), responder.capabilities.end(), incident.required_capability) != responder.capabilities.end()) {
            suitable_responders.push_back(&responder);
        }
    }

    if (suitable_responders.empty()) {
        return false; // No suitable responders available
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
        best_responder->availability = AvailabilityStatus::BUSY;
        best_responder->time_until_available = best_time + incident.time_required; 
        best_responder->start_location = incident.location; // Update responder location

        // Construct the document matching the required plan.json schema
        Assignment assignment;
        assignment.responder_id = best_responder->responder_id;
        assignment.incident_id = incident.incident_id;
        assignment.path = best_path_ids;
        assignment.time = incident.timestamp; // Or current simulation tick if tracked globally

        tick_document doc;
        doc.assignments.push_back(assignment);

        Documentor documentor;
        documentor.document(doc);

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
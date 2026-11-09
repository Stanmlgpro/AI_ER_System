#ifndef INCIDENTS_H
#define INCIDENTS_H

#include <string>
#include <vector>
#include "enums.h"

struct Incident {
    std::string incident_id;
    std::string location; // Node ID[cite: 3]
    IncidentPriorityLevel priority; // Critical, High, Medium, Low[cite: 3]
    double deadline; // Absolute simulation time[cite: 3]
    std::string required_capability; //[cite: 3]
    double timestamp; // When the incident becomes known[cite: 3]
    double time_required; // Duration needed post-arrival[cite: 3]
};

std::vector<Incident> load_incidents(const std::string& filepath);

#endif // INCIDENTS_H
#ifndef INCIDENTS_H
#define INCIDENTS_H

#include <string>
#include <vector>
#include "enums.h"

struct Incident {
    std::string incident_id;
    std::string location;
    IncidentPriorityLevel priority;
    double deadline;
    std::string required_capability;
    int timestamp;
    double time_required;
};

std::vector<Incident> load_incidents(const std::string& filepath);

#endif // INCIDENTS_H
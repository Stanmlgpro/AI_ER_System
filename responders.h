#ifndef RESPONDERS_H
#define RESPONDERS_H

#include <string>
#include <vector>

struct Responder {
    std::string responder_id;
    std::string type; // Ambulance, PoliceUnit, FireTruck[cite: 2]
    std::string start_location; // Node ID[cite: 2]
    std::vector<std::string> capabilities; // e.g., "Medical emergency", "CPR"[cite: 2]
    std::string availability; // "Available" or "Busy"[cite: 2]
    double time_until_available; // Simulation time units[cite: 2]
};

#endif // RESPONDERS_H
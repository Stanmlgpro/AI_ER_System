#ifndef RESPONDERS_H
#define RESPONDERS_H

#include <string>
#include <vector>
#include "enums.h"

struct Responder {
    std::string responder_id;
    ResponderType type;
    std::string start_location;
    std::vector<std::string> capabilities;
    AvailabilityStatus availability;
    double time_until_available;
};

std::vector<Responder> load_responders(const std::string& filepath);

#endif // RESPONDERS_H
#ifndef EMULATOR_H
#define EMULATOR_H

#include "city_graph.h"
#include "incidents.h"
#include "responders.h"

class Emulator {
    public:
        Emulator(CityGraph* city_, const std::vector<Incident>& incidents_, const std::vector<Responder>& responders_);
        void run_tick(int current_time);

        ~Emulator();

    private:
        CityGraph* city;
        std::vector<Incident> incidents;
        std::vector<Incident> unresolved_incidents;
        std::vector<Responder> responders;

        bool find_responder_for_incident(Incident& incident, std::vector<Responder>& available_responders);
};

#endif // EMULATOR_H
#ifndef EMULATOR_H
#define EMULATOR_H

#include "city_graph.h"
#include "incidents.h"
#include "responders.h"
#include "documentor.h"

class Emulator {
    public:
        Emulator(Documentor& documentor, CityGraph* city_, const std::vector<Incident>& incidents_, const std::vector<Responder>& responders_);
        void run_tick(int current_time);
        bool check_end_conditions() const;

        ~Emulator();

    private:
        Documentor& documentor;
        CityGraph* city;
        std::vector<Incident> incidents;
        std::vector<Incident> unresolved_incidents;
        std::vector<Responder> responders;

        bool find_responder_for_incident(Incident& incident, std::vector<Responder>& available_responders, int current_time);
};

#endif // EMULATOR_H
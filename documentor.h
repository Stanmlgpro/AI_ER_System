#ifndef DOCUMENTER_H
#define DOCUMENTER_H
#include <string>
#include <vector>

struct Assignment {
    std::string responder_id;
    std::string incident_id;
    std::vector<std::string> path;
    int time;
};

struct tick_document {
    std::vector<Assignment> assignments;
};

class Documentor {
public:
    // Accumulates data from each call
    Documentor(int total_num_incidents) : total_num_incidents_(total_num_incidents) {}
    void document(const tick_document& doc, int current_time);
    void document_response(double response_time);

    // Writes everything accumulated so far to the files
    void generate_documentation();

private:
    // Internal storage
    std::vector<Assignment> all_assignments_; // Adjust type name based on your codebase
    std::vector<std::string> responder_ids;
    size_t total_dispatches_ = 0;
    int current_time_ = 0;
    int total_num_incidents_ = 0;
    double total_response_time_ = 0.0;
};

#endif // DOCUMENTER_H
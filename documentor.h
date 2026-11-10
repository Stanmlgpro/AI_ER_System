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
    Documentor() = default;

    void document(const tick_document& doc);

    static void generate_documentation();
};

#endif // DOCUMENTER_H
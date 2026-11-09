#ifndef ENUMS_H
#define ENUMS_H

enum class ResponderType {
    FIRE,
    AMBULANCE,
    POLICE
};

enum class RoadState {
    OPEN,
    CONGESTED,
    BLOCKED
};

enum class WeatherCondition {
    CLEAR,
    RAIN,
    STORM
};

enum class RoadStateSources {
    CITIZENS,
    POLICE,
    SENSORS
};

enum class AvailabilityStatus {
    AVAILABLE,
    BUSY
};

enum class IncidentPriorityLevel {
    CRITICAL,
    HIGH,
    MEDIUM,
    LOW
};

#endif // ENUMS_H

#pragma once

#include <vector>
#include <cstdint>

// Corresponds to the "first" object in the JSON command
struct Group1 {
    double x = -1.0, z = -1.0, c = -1.0;
    uint64_t vx = 0, vz = 0, vc = 0;
};

// Corresponds to the "second" object in the JSON command
struct Group2 {
    double f = -1.0, g = -1.0;
    uint64_t vf = 0, vg = 0;
};

// Represents a single point in the route, containing one or both groups
struct Aggregated {
    Group1 first;
    Group2 second;
    bool has_first = false;
    bool has_second = false;
    bool enabled = false;
};

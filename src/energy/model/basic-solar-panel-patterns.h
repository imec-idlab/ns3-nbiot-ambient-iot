#ifndef BASIC_SOLAR_PANEL_PATTERNS_H
#define BASIC_SOLAR_PANEL_PATTERNS_H

#include <vector>

// Define the structure for pattern data
struct SolarPattern {
    long second;
    double mean;
    double std_dev;
};


#define SECONDS_IN_A_DAY 86400

// Declare the function signature
std::vector<SolarPattern> * generate_day(long N = SECONDS_IN_A_DAY);


#endif // BASIC_SOLAR_PANEL_PATTERNS_H
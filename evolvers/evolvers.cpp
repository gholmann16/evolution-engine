#include <state.hpp>
#include <algorithm>
#include "standard.hpp"
#include "squarelite.cpp"
#include "above_average.cpp"
#include "top_10_percent.cpp"

Evolver * evolvers[] {
    new Squarelite(),
    new Above_Average(),
    new Top_10_Percent(),
};

const char * evolver_names[] {
    "Squarelite",
    "Above_Average",
    "Top_10_Percent",
};

int num_evolvers = 3;

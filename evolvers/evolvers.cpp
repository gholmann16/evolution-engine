#include <state.hpp>
#include <algorithm>
#include "squarelite.cpp"
#include "above_average.cpp"

Evolver * evolvers[] {
    new Squarelite(),
    new Above_Average(),
};

const char * evolver_names[] {
    "Squarelite",
    "Above_Average",
};

int num_evolvers = 2;

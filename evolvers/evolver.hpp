#pragma once
#include "../engines/engine.hpp"
#include "../tests/test.hpp"
#include <algorithm>

class Evolver {
    public:
        static bool compare_ratings(const Program& first, const Program &second) {
            return first.score < second.score;
        }

        // not sure what to return yet
        virtual unsigned long long evolve(Test * tester, Engine * engine, const struct State state) = 0;

        // returns total score
        virtual unsigned long long score_all(Test * tester, Engine * engine, const struct State state) = 0;

        // returns best score
        virtual unsigned long long sort(Test * tester, Engine * engine, const struct State state) = 0;
};

Evolver * create_squarelite();
Evolver * create_above_average();

#pragma once
#include "../engines/engine.hpp"
#include "../tests/test.hpp"
#include <algorithm>

class Evolver {
    public:
        static bool compare_ratings(const Program& first, const Program &second) {
            return first.score < second.score;
        }

        virtual void evolve(Test * tester, Engine * engine, const struct State state) = 0;
        virtual void score_all(Test * tester, Engine * engine, const struct State state) = 0;
        virtual void sort(Test * tester, Engine * engine, const struct State state) = 0;
};

Evolver * create_squarelite(Test * tester, Engine * engine, const struct State state);
Evolver * create_above_average(Test * tester, Engine * engine, const struct State state);

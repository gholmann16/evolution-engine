#include "evolver.hpp"
#include <math.h>

class Above_Average : public Evolver {
    private:
        int breeders;
    public:
        Above_Average(Test * tester, Engine * engine, const struct State state) {
            for (int i = state.total_creatures / 2; i < state.total_creatures; i++)
                state.children[i].score = tester->score(state.children[i].code, engine, &state);
        }

        void evolve(Test * tester, Engine * engine, const struct State state) {
            int max_reps = state.def_rand - 50;
            breeders = state.total_creatures / 2 - state.repetitions * state.total_creatures / max_reps;
            // Keep the winner around so you never regress (agamogenesis) and reset
            for (int creature = breeders; creature < state.total_creatures; creature++)
                engine->evolve(state.children[creature % breeders].code, state.children[creature].code, state.def_rand - state.repetitions);
        }

        void score_all(Test * tester, Engine * engine, const struct State state) {
            for (int i = breeders; i < state.total_creatures; i++) {
                state.children[i].score = tester->score(state.children[i].code, engine, &state);
                printf("%lld\t", state.children[i].score);
            }
        }

        void sort(Test * tester, Engine * engine, const struct State state) {
            std::sort(state.children, state.children + state.total_creatures, compare_ratings);
            // Stir winning pool, shoot for diversity
            // for (int elite = 0; elite < state.total_creatures / 2; elite++) {
            //     if (rand() % ((state.total_creatures - elite) / (int)sqrt(state.total_creatures) + 2)) {
            //         void * tmp = state.children[elite].code;
            //         state.children[elite].code = state.children[state.total_creatures - elite - 1].code;
            //         state.children[state.total_creatures - elite - 1].code = tmp;
            //     }
            // }
        }
};

Evolver * create_above_average(Test * tester, Engine * engine, const struct State state) {
    return new Above_Average(tester, engine, state);
}

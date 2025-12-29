#include "evolver.hpp"
#include <math.h>

class Above_Average : public Evolver {
    public:
        unsigned long long evolve(Test * tester, Engine * engine, const struct State state) {
            // Evolve from winning pool
            for (int winner = 0; winner < state.total_creatures / 2; winner++) {
                // Keep the winner around so you never regress (agamogenesis) and reset
                state.children[winner].score = 0;

                // Other grandchildren slightly modified, reset by evolve()
                engine->evolve(state.children[winner].code, state.children[state.total_creatures / 2 + winner].code, state.def_rand - state.repetitions);
                state.children[state.total_creatures / 2 + winner].score = 0;
            }
            return 0;
        }

        unsigned long long score_all(Test * tester, Engine * engine, const struct State state) {
            unsigned long long total_score = 0;
            for (int i = 0; i < state.total_creatures; i++) {
                state.children[i].score = tester->score(state.children[i].code, engine, &state);
                printf("%lld\t", state.children[i].score);
                total_score += state.children[i].score;
            }
            return total_score;
        }

        unsigned long long sort(Test * tester, Engine * engine, const struct State state) {
            std::sort(state.children, state.children + state.total_creatures, compare_ratings);
            unsigned long long best = state.children[0].score;
            // Stir winning pool, shoot for diversity
            for (int elite = 0; elite < state.total_creatures / 2; elite++) {
                if (rand() % ((state.total_creatures - elite) / (int)sqrt(state.total_creatures) + 2)) {
                    void * tmp = state.children[elite].code;
                    state.children[elite].code = state.children[state.total_creatures - elite - 1].code;
                    state.children[state.total_creatures - elite - 1].code = tmp;
                }
            }
            return best;
        }
};

Evolver * create_above_average() {
    return new Above_Average();
}

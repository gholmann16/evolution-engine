// squarelite -> square elite, the square root gets prioritized and passes on genes each generation
#include "evolver.hpp"
#include <math.h>

class Squarelite : public Evolver {
    private:
        int square = 0;
    public:
        unsigned long long evolve(Test * tester, Engine * engine, const struct State state) {
            if (!square)
                square = (int)sqrt(state.total_creatures);

            // Evolve from winning pool
            for (int winner = 0; winner < square; winner++) {
                // Keep the winner around so you never regress (agamogenesis) and reset
                // Other grandchildren slightly modified, reset by evolve()
                for (int grandchild = 1; grandchild < square; grandchild++) {
                    engine->evolve(state.children[winner].code, state.children[grandchild * square + winner].code, state.def_rand - state.repetitions);
                    state.children[grandchild * square + winner].score = 0;
                }
            }
            return 0;
        }

        unsigned long long score_all(Test * tester, Engine * engine, const struct State state) {
            unsigned long long total_score = 0;
            // no reason to re-test the winners since we're not changing them
            for (int i = square; i < state.total_creatures; i++) {
                state.children[i].score = tester->score(state.children[i].code, engine, &state);
                printf("%lld\t", state.children[i].score);
                total_score += state.children[i].score;
            }
            return total_score;
        }

        unsigned long long sort(Test * tester, Engine * engine, const struct State state) {
            std::sort(state.children, state.children + state.total_creatures, compare_ratings);

            // Get winning pool, shoot for diversity
            int found = 1;
            for (int candidate = 1; candidate < state.total_creatures && found < square; candidate++) {
                /* 
                * If we haven't found all winners, and this one is not identical to the previous winner
                * Can still have duplicates probably but not all duplicates, at least some genetic variety
                * If they're all the same hypothetically, then some left over dna from last generation would stay
                * Conveniently, the worse dna of the batch because the winners will stay winners
                * Frees everything we need, since parent and children share a pool
                */
                if (engine->equal(state.children[candidate].code, state.children[found - 1].code) == false)
                    state.children[found++] = state.children[candidate];
            }
            while (found < square)
                state.children[found++] = state.children[0];
            return state.children[0].score;
        }
};

Evolver * create_squarelite() {
    return new Squarelite();
}

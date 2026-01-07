// squarelite -> square elite, the square root gets prioritized and passes on genes each generation
#include <math.h>

class Squarelite : public Evolver {
    private:
        inline static size_t square;
        static void calc_square() {
            square = (size_t)sqrt(State::total_creatures);
        }
    public:
        void evolve() const {
            calc_square();
            Standard::evolve(square);
        }

        void score_all() const {
            Standard::score_all(square);
        }

        void sort() const {
            std::sort(State::children, State::children + State::total_creatures, compare_ratings);
            // Get winning pool, shoot for diversity
            size_t found = 1;
            for (size_t candidate = 1; candidate < State::total_creatures && found < square; candidate++) {
                /* 
                * If we haven't found all winners, and this one is not identical to the previous winner
                * Can still have duplicates probably but not all duplicates, at least some genetic variety
                * If they're all the same hypothetically, then some left over dna from last generation would stay
                * Conveniently, the worse dna of the batch because the winners will stay winners
                * Frees everything we need, since parent and children share a pool
                */
                if (State::engine->equal(State::children[candidate].code, State::children[found - 1].code) == false) {
                    // it won't be read again unless candidate will be overwritten too, in which case it might reduce some diversity
                    struct Program tmp;
                    tmp = State::children[found];
                    State::children[found] = State::children[candidate];
                    State::children[candidate] = tmp;
                    found++;
                }
            }
            while (found < square) {
                State::children[found].score = State::children[0].score;
                State::engine->copy_into(State::children[0].code, State::children[found].code);
                found++;
            }
        }
};

class Above_Average : public Evolver {
    private:
        // Shared by evolve()/score_all() so the elite cutoff -- the range
        // left untouched (code and score both) -- is identical for both:
        // whatever evolve() freshly mutates, score_all() rescores, and
        // nothing else. Previously score_all() used its own formula that
        // shrank half as fast as this one, so as repetitions grew, a
        // widening range of indices got freshly mutated code paired with a
        // stale, pre-mutation score until repetitions reset to 0.
        static size_t elite_cutoff() {
            return State::total_creatures / 2 - State::repetitions * State::total_creatures / clamped_sub(State::def_rand, 50);
        }
    public:
        void evolve() const {
            Standard::evolve(elite_cutoff());
        }

        void score_all() const {
            Standard::score_all(elite_cutoff());
        }

        void sort() const {
            std::sort(State::children, State::children + State::total_creatures, compare_ratings);
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

class Above_Average : public Evolver {
    public:
        void evolve() const {
            Standard::evolve(State::total_creatures / 2 - State::repetitions * State::total_creatures / (State::def_rand - 50));
        }

        void score_all() const {
            Standard::score_all(State::total_creatures / 2 - State::repetitions * State::total_creatures / (State::def_rand - 50));
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

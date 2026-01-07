class Above_Average : public Evolver {
    private:
        inline static size_t breeders;
        
        static void calculate_breeders() {
            breeders = State::total_creatures / 2 - State::repetitions * State::total_creatures / (State::def_rand - 50);
        }

    public:

        void evolve() const {
            calculate_breeders();
            // Keep the winner around so you never regress (agamogenesis) and reset
            for (size_t creature = breeders; creature < State::total_creatures; creature++)
                State::engine->evolve(State::children[creature % breeders].code, State::children[creature].code, State::def_rand - State::repetitions);
        }

        void score_all() const {
            for (size_t i = breeders; i < State::total_creatures; i++) {
                State::children[i].score = State::test->score(State::children[i].code);
                printf("%zu\t", State::children[i].score);
            }
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

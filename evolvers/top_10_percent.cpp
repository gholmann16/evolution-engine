class Top_10_Percent : public Evolver {
    public:
        void evolve() const {
            Standard::evolve(State::total_creatures / 10);
        }

        void score_all() const {
            Standard::score_all(State::total_creatures / 10);
        }

        void sort() const {
            std::sort(State::children, State::children + State::total_creatures, compare_ratings);
        }
};

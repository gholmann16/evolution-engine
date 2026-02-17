#pragma once
namespace Standard {
    void evolve(int mod) {
        // Keep the winner around so you never regress (agamogenesis) and reset
        for (size_t creature = mod; creature < State::total_creatures; creature++)
            State::engine->evolve(State::children[creature % mod].code, State::children[creature].code, State::def_rand - State::repetitions);
    }

    void score_all(int mod) {
        // only need to score the new creatures
        for (size_t i = mod; i < State::total_creatures; i++) {
            State::children[i].score = State::test->score(State::children[i].code);
            if (State::verbose)
                printf("%zu\t", State::children[i].score);
        }
    }
}

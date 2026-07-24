#include "tictactoe_common.hpp"

// 1 v 1 tic tac toe, self-play against the hall of fame
class Tic_Off : public Test {
    public:
        size_t score(const void * code) const override {
            return 50 * hof_cost(code) + State::engine->size(code);
        }

        std::string display(const void * code) const override {
            int wins = hof_wins(code);
            return std::to_string(wins) + "/" + std::to_string(State::total_famers * 2) + " games won";
        }
};

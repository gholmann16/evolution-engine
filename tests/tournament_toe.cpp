#include "tictactoe_common.hpp"

// Tic_Off's hall-of-fame self-play plus TicTacToe's exhaustive search --
// the hall of fame gets it off the ground, the exhaustive half pushes it
// toward a genuinely perfect program. total_famers=100 -> 200 + 1329 = 1529 games.
class Tournament_Toe : public Test {
    public:
        size_t score(const void * code) const override {
            int hof = hof_cost(code);

            alignas(256) char output[256] = {0};
            alignas(256) char board[256] = "         ";
            alignas(256) char board2[256] = "         ";
            State::engine->load(code);
            first_turn_x(board, output);
            size_t lost = place(board, output[0], 'X') ? 384 : exhaustive_play(board, output, 8, 'X', 'O');
            lost += exhaustive_play(board2, output, 9, 'O', 'X');

            if (exhaustive_runtime >= State::max_runtime)
                return State::max_runtime * 50;
            return (hof + lost) * 10000 + exhaustive_runtime + State::engine->size(code) * 10;
        }

        std::string display(const void * code) const override {
            int wins = hof_wins(code);

            alignas(256) char output[256] = {0};
            alignas(256) char board[256] = "         ";
            alignas(256) char board2[256] = "         ";
            State::engine->load(code);
            exhaustive_wins = 0;
            first_turn_x(board, output);
            if (!place(board, output[0], 'X'))
                exhaustive_play(board, output, 8, 'X', 'O');
            exhaustive_play(board2, output, 9, 'O', 'X');

            wins += exhaustive_wins;
            size_t total_games = State::total_famers * 2 + 1329;
            return std::to_string(wins) + "/" + std::to_string(total_games) + " games won";
        }
};

#include <string.h>
#include <iostream>

#define REPS 100

class TicTacToe : public Test {
    private:
        static bool won(char board[256], char player) {
            if (
                (board[0] == player && board[1] == player && board[2] == player) ||
                (board[3] == player && board[4] == player && board[5] == player) ||
                (board[6] == player && board[7] == player && board[8] == player) ||
                (board[0] == player && board[3] == player && board[6] == player) ||
                (board[1] == player && board[4] == player && board[7] == player) ||
                (board[2] == player && board[5] == player && board[8] == player) ||
                (board[0] == player && board[4] == player && board[8] == player) ||
                (board[2] == player && board[4] == player && board[6] == player)
            ) {
                // printf("%c | %c | %c\n", board[0], board[1], board[2]);
                // printf("%c | %c | %c\n", board[3], board[4], board[5]);
                // printf("%c | %c | %c\n", board[6], board[7], board[8]);

                // printf("%c Won!\n", player);
                return true;
            }
            else
                return false;
        }

        static bool place(char board[256], unsigned char spot, char player) {
            if (spot >= 9)
                return true;
            if (board[spot] == ' ') {
                board[spot] = player;
                return false;
            }
            return true;
        }

        inline static bool training_mode = false;

        // thread_local: mutated on every score() call (reset then accumulated
        // across the recursive play_game() calls within that one call), so
        // concurrent score() calls from the scoring thread pool need their
        // own counter, not one shared across threads.
        inline static thread_local size_t total_runtime;
        static size_t play_game(char board[256], char output[256], int empty, char player, char other) {
            alignas(256) char save_board[256];
            size_t total_recursive_score = 0;
            size_t games_left[] = {1, 1, 1, 2, 3, 8, 15, 48, 105};

            for (int i = 0; i < 9 && total_runtime <= State::max_runtime; i++) {
                if (board[i] != ' ')
                    continue;
                memcpy(save_board, board, 64);
                save_board[i] = other;
                if (won(save_board, other)) {
                    total_recursive_score += games_left[empty - 1];
                    continue;
                }

                total_runtime += State::engine->run(save_board, output, State::max_runtime - total_runtime);
                if (place(save_board, output[0], player)) {
                    total_recursive_score += games_left[empty - 1];
                    continue;
                }

                if (won(save_board, player))
                    total_recursive_score += 0;
                else if (empty)
                    total_recursive_score += play_game(save_board, output, empty - 2, player, other);
            }
            return total_recursive_score;
        }

        static void first_turn(char board[256], char output[256]) {
            total_runtime = State::engine->run(board, output, State::max_runtime);
        }

        static void train(void * code) {
            training_mode = true;

        }

        // +1 = first-mover wins, 0 = tie, -1 = second-mover wins. A concrete
        // single-game playthrough, unlike play_game()'s exhaustive minimax
        // search over every possible opponent line -- this is for display()
        // only (a "wins out of games played" count), not part of the actual
        // scoring/evolution objective.
        static int fight(Engine * first, Engine * second) {
            alignas(256) char board[256] = "         ";
            alignas(256) char output[256];
            for (int turn = 0; turn < 9; turn++) {
                Engine * mover = (turn % 2 == 0) ? first : second;
                char mark = (turn % 2 == 0) ? 'X' : 'O';
                if (mover->run(board, output, State::max_runtime) == State::max_runtime || place(board, output[0], mark))
                    return (turn % 2 == 0) ? -1 : 1;   // mover forfeited -- other side wins
                if (won(board, mark))
                    return (turn % 2 == 0) ? 1 : -1;
            }
            return 0;   // board filled with no winner
        }

    #define LEARNING 0.05f
    public:
        size_t score(const void * code) const override {
            alignas(256) char output[256] = {0};
            alignas(256) char board[256] = "         ";
            alignas(256) char board2[256] = "         ";

            State::engine->load(code);
            first_turn(board, output);
            size_t lost = place(board, output[0], 'X') ? 384 : play_game(board, output, 8, 'X', 'O');
            if (State::verbose)
                printf("(%zu)\t", lost);
            lost += play_game(board2, output, 9, 'O', 'X');

            if (State::verbose)
                printf("(%zu)\t", lost);
            // printf("total_runtime = %zu\n", total_runtime);
            if (total_runtime >= State::max_runtime)
                return State::max_runtime * 50;
            else if (lost)
                return lost * 10000 + total_runtime + State::engine->size(code) * 10;
            else
                return 0;
        }

        // Concrete games (via fight()) against the whole hall of fame,
        // purely to report a "wins out of games played" count.
        std::string display(const void * code) const override {
            State::engine->load(code);
            int wins = 0;
            for (size_t i = 0; i < State::total_famers; i++) {
                State::comp->load(State::hall_of_fame[i]);
                if (fight(State::engine, State::comp) == 1) wins++;
                if (fight(State::comp, State::engine) == -1) wins++;
            }
            return std::to_string(wins) + "/" + std::to_string(State::total_famers * 2) + " games won";
        }
};

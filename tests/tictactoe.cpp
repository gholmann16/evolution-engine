#include <string.h>
#include <iostream>

#define MAX_TIC_TAC_TOE 10000000
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

        inline static size_t total_runtime;
        static size_t play_game(char board[256], char output[256], int empty, char player, char other) {
            alignas(256) char save_board[256];
            size_t total_recursive_score = 0;
            size_t games_left[] = {1, 1, 1, 2, 3, 8, 15, 48, 105};

            for (int i = 0; i < 9 && total_runtime <= MAX_TIC_TAC_TOE; i++) {
                if (board[i] != ' ')
                    continue;
                memcpy(save_board, board, 64);
                save_board[i] = other;
                if (won(save_board, other)) {
                    total_recursive_score += games_left[empty - 1];
                    continue;
                }

                total_runtime += State::engine->run(save_board, output, MAX_TIC_TAC_TOE - total_runtime);
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
            total_runtime = State::engine->run(board, output, MAX_TIC_TAC_TOE);
        }

        static void train(void * code) {
            training_mode = true;

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
            if (total_runtime >= MAX_TIC_TAC_TOE)
                return MAX_TIC_TAC_TOE * 50;
            else if (lost)
                return lost * 10000 + total_runtime + State::engine->size(code) * 10;
            else
                return 0;
        }
};

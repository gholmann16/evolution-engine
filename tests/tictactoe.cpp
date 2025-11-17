#include <evolver.hpp>
#include <string.h>
#include <iostream>

#define MAX_RUNTIME 100
#define REPS 100

class TicTacToe : public Test {
    private:
        bool won(char board[256], char player) {
            if (
                (board[0] == player && board[1] == player && board[2] == player) ||
                (board[3] == player && board[4] == player && board[5] == player) ||
                (board[6] == player && board[7] == player && board[8] == player) ||
                (board[0] == player && board[3] == player && board[6] == player) ||
                (board[1] == player && board[4] == player && board[7] == player) ||
                (board[2] == player && board[5] == player && board[8] == player) ||
                (board[0] == player && board[4] == player && board[8] == player) ||
                (board[2] == player && board[4] == player && board[6] == player)
            )
                return true;
            else
                return false;
        }

        void place(char board[256], unsigned char spot, char player) {
            while (spot >= 9 || board[spot] != ' ')
                spot = (spot + 1) % 9;
            board[spot] = player;
        }

        size_t total_runtime;
        size_t play_game(struct Program * prog, Engine * engine, char board[256], char output[256]) {
            alignas(256) char save_board[256];
            int total_recursive_score = 0;
            for (int i = 0; i < 9 && total_runtime <= MAX_RUNTIME; i++) {
                if (board[i] != ' ')
                    continue;
                // printf("games %ld\n", played_games);
                memcpy(save_board, board, 9);
                save_board[i] = 'O';
                if (won(save_board, 'O')) {
                    // printf("AI lost:\n");
                    // printf("%c | %c | %c\n", save_board[0], save_board[1], save_board[2]);
                    // printf("%c | %c | %c\n", save_board[3], save_board[4], save_board[5]);
                    // printf("%c | %c | %c\n", save_board[6], save_board[7], save_board[8]);
                    total_recursive_score++;
                    continue;
                }
                total_runtime += engine->run(prog->code, save_board, output, MAX_RUNTIME - total_runtime);
                // printf("%c | %c | %c\n", save_board[0], save_board[1], save_board[2]);
                // printf("%c | %c | %c\n", save_board[3], save_board[4], save_board[5]);
                // printf("%c | %c | %c\n", save_board[6], save_board[7], save_board[8]);
                place(save_board, output[0], 'X');
                if (won(save_board, 'X'))
                    total_recursive_score += 0;
                else
                    total_recursive_score += play_game(prog, engine, save_board, output);
            }
            return total_recursive_score;
        }
    public:
        void prepare_answer() override {
            ;
        }

        void score(struct Program * prog, Engine * engine) override {
            alignas(256) char output[256] = {0};
            alignas(256) char board[256] = {0};
            for (int spot = 0; spot < 9; spot++)
                board[spot] = ' ';
            total_runtime = engine->run(prog->code, board, output, MAX_RUNTIME);
            place(board, output[0], 'X');
            size_t lost = play_game(prog, engine, board, output);

            if (total_runtime >= MAX_RUNTIME) {
                prog->score = MAX_RUNTIME * 50;
                return;
            }

            if (lost)
                prog->score = lost * 50 + prog->code.size();
            else
                prog->score = 0;
        }

        void display(struct Program * prog, Engine * engine) override {
            ;
        }

        const char * allowed_chars() override {
            return "+-<>[].,";
        }
};

Test * create_tictactoe() {
    return new TicTacToe();
}

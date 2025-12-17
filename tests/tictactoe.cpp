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

        bool place(char board[256], unsigned char spot, char player) {
            if (board[spot] == ' ') {
                board[spot] = player;
                return false;
            }
            return true;
        }

        size_t total_runtime;
        size_t play_game(struct Program * prog, Engine * engine, char board[256], char output[256], int empty) {
            alignas(256) char save_board[256];
            int total_recursive_score = 0;

            for (int i = 0; i < 9 && total_runtime <= MAX_RUNTIME; i++) {
                if (board[i] != ' ')
                    continue;
                memcpy(save_board, board, 9);
                save_board[i] = 'O';
                if (won(save_board, 'O')) {
                    total_recursive_score++;
                    continue;
                }
                total_runtime += engine->run(prog->code, save_board, output, MAX_RUNTIME - total_runtime);
                if (place(save_board, output[0], 'X')) {
                    // couldn't place it, so guaranteed did not win, also guaranteed not empty
                    switch(empty) {
                        case 8: // fucked up on the first turn, so essentially every move set starting by the second should count as lost
                            total_recursive_score += 48;
                            continue;
                        case 6:
                            total_recursive_score += 8;
                            continue;
                        case 4: // both games that would have ensued had this gone through are done
                            total_recursive_score += 2;
                            continue;
                        case 2: // no games would be played, so this is a normal loss
                            total_recursive_score += 1;
                            continue;
                    }
                }
                
                if (won(save_board, 'X'))
                    total_recursive_score += 0;
                else if (empty)
                    total_recursive_score += play_game(prog, engine, save_board, output, empty - 2);
            }
            return total_recursive_score;
        }
    public:
        void prepare_answer() override {
            ;
        }

        void score(struct Program * prog, Engine * engine) override {
            alignas(256) char output[256] = {0};
            alignas(256) char board[256] = "         ";
            size_t lost;
            total_runtime = engine->run(prog->code, board, output, MAX_RUNTIME);
            if (place(board, output[0], 'X')) {
                lost = 384;
            }
            else {
                lost = play_game(prog, engine, board, output, 8);
            }

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

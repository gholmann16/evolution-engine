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

        inline static size_t total_runtime;
        static size_t play_game(const void * code, char board[256], char output[256], int empty, char player, char other) {
            alignas(256) char save_board[256];
            int total_recursive_score = 0;

            for (int i = 0; i < 9 && total_runtime <= MAX_TIC_TAC_TOE; i++) {
                if (board[i] != ' ')
                    continue;
                memcpy(save_board, board, 9);
                save_board[i] = other;
                if (won(save_board, other)) {
                    total_recursive_score++;
                    continue;
                }
                if (empty == 1)
                    continue; // aka total_score += 0

                total_runtime += State::engine->run(code, save_board, output, MAX_TIC_TAC_TOE - total_runtime);
                if (place(save_board, output[0], player)) {
                    // couldn't place it, so guaranteed did not win, also guaranteed not empty
                    switch(empty) {
                        case 9:
                            total_recursive_score += 105;
                            continue;
                        case 8: // fucked up on the first turn, so essentially every move set starting by the second should count as lost
                            total_recursive_score += 48;
                            continue;
                        case 7:
                            total_recursive_score += 15;
                            continue;
                        case 6:
                            total_recursive_score += 8;
                            continue;
                        case 5:
                            total_recursive_score += 3;
                            continue;
                        case 4: // both games that would have ensued had this gone through are done
                            total_recursive_score += 2;
                            continue;
                        case 3:
                            total_recursive_score += 1;
                            continue;
                        case 2: // no games would be played, so this is a normal loss
                            total_recursive_score += 1;
                            continue;
                    }
                }

                if (won(save_board, player))
                    total_recursive_score += 0;
                else if (empty)
                    total_recursive_score += play_game(code, save_board, output, empty - 2, player, other);
            }
            return total_recursive_score;
        }

        static void first_turn(const void * code, char board[256], char output[256]) {
            total_runtime = State::engine->run(code, board, output, MAX_TIC_TAC_TOE);
        }

    public:
        size_t score(const void * code) const override {
            alignas(256) char output[256] = {0};
            alignas(256) char board[256] = "         ";
            alignas(256) char board2[256] = "         ";
            first_turn(code, board, output);
            // printf("total_runtime = %zu\n", total_runtime);
            size_t lost = place(board, output[0], 'X') ? 384 : play_game(code, board, output, 8, 'X', 'O');
            // printf("total_runtime = %zu\n", total_runtime);
            lost += play_game(code, board2, output, 9, 'O', 'X');

            if (State::verbose)
                printf("(%zu)\t", lost);
            // printf("total_runtime = %zu\n", total_runtime);
            if (total_runtime >= MAX_TIC_TAC_TOE)
                return MAX_TIC_TAC_TOE * 50;
            else if (lost)
                return lost * 1000 + total_runtime;
            else
                return 0;
        }
};

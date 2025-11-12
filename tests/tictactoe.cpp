#include "test.hpp"

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
        
        int find_winner(char board[256]) {
            if (won(board, 'X'))
                return 1;
            else if (won(board, 'O'))
                return -1;
            else
                return 0;
        }

        void place(char board[256], unsigned char spot, char player) {
            while (spot >= 9 || board[spot] == 'X' || board[spot] == 'O')
                spot = rand() % 9;
            board[spot] = player;
        }

        int play_game(struct Program * prog, char board[256], char output[256]) {
            for (int spot = 0; spot < 9; spot++)
                board[spot] = ' ';

            run(prog, board, output, MAX_RUNTIME);
            if (prog->runtime == MAX_RUNTIME)
                return MAX_RUNTIME;
            // probably faster to check 3 spots for x then compare with each other then check anyway lol
            place(board, output[0], 'X');

            for (int i = 0; i < 4; i++) { // first 3 turns no winers
                place(board, 255, 'O');
                if (won(board, 'O'))
                    return 5;
                run(prog, board, output, MAX_RUNTIME);
                place(board, output[0], 'X');
                if (prog->runtime)
                    return MAX_RUNTIME;
                else if (won(board, 'X'))
                    return 0;
            }
            return 1;
        }
    public:
        void prepare_answer() override {
            ;
        }

        void score(struct Program * prog) override {
            alignas(256) char output[256];

            int total = 0;
            for (int reps = 0; reps < REPS; reps++) {
                alignas(256) char board[256] = {0};

                int add = play_game(prog, board, output);
                if (add > MAX_RUNTIME) {
                    prog->score = MAX_RUNTIME * 50;
                    return;
                }
                total += add;
            }

            if (total)
                prog->score = total * 50 + prog->size;
            else
                prog->score = 0;
        }

        void display(struct Program * prog) override {
            ;
        }

        const char * allowed_chars() override {
            return "+-<>[].,";
        }
};

Test * create_tictactoe() {
    return new TicTacToe();
}

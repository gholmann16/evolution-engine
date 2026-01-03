#include "test.hpp"
#define MAX_RUNTIME 1000000

// 1 v 1 tic tac toe
class Tic_Off : public Test {
    private:
        bool won(char board[256]) {
            if (
                (board[0] == 'X' && board[1] == 'X' && board[2] == 'X') ||
                (board[3] == 'X' && board[4] == 'X' && board[5] == 'X') ||
                (board[6] == 'X' && board[7] == 'X' && board[8] == 'X') ||
                (board[0] == 'X' && board[3] == 'X' && board[6] == 'X') ||
                (board[1] == 'X' && board[4] == 'X' && board[7] == 'X') ||
                (board[2] == 'X' && board[5] == 'X' && board[8] == 'X') ||
                (board[0] == 'X' && board[4] == 'X' && board[8] == 'X') ||
                (board[2] == 'X' && board[4] == 'X' && board[6] == 'X')
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

        bool place(char board[256], unsigned char spot) {
            if (board[spot] == ' ') {
                board[spot] = 'X';
                return false;
            }
            return true;
        }

        void flip(char board[256]) {
            for (int i = 0; i < 9; i++) {
                switch(board[i]) {
                    case 'X':
                        board[i] = 'O';
                        break;
                    case 'O':
                        board[i] = 'X';
                        break;
                }
            }
        }

        /*
        +1 - first wins
         0 - tie
        -1 - second wins
        We actually need to flip each time, because if given a board state at random
        where you don't know who you are, then how are you supposed to make the right choice?
        Tester is always scoring for X so we simulate that with flip()
        */ 
        // 
        int fight(const void * first, const void * second, Engine * engine) {
            alignas(256) char board[256] = "         ";
            alignas(256) char output[256];
            if (engine->run(first, board, output, MAX_RUNTIME) == MAX_RUNTIME || place(board, output[0]))
                return -1;
            for (int i = 0; i < 4; i++) {
                flip(board);
                if (engine->run(second, board, output, MAX_RUNTIME) == MAX_RUNTIME || place(board, output[0]))
                    return 1;
                if (won(board))
                    return -1;
                flip(board);
                if (engine->run(first, board, output, MAX_RUNTIME) == MAX_RUNTIME || place(board, output[0]))
                    return -1;
                if (won(board))
                    return 1;
            }
            return 0;
        }

    public:
        unsigned long long score(const void * code, Engine * engine, const State * state) {
            // 100 winners of last generation, who don't play cause they already have a score
            unsigned long long total = HALL_OF_FAMERS * 2;
            for (int i = 0; i < HALL_OF_FAMERS; i++) {
                total -= fight(code, state->hall_of_fame[i], engine); // if first wins, subtract 1 (better)
                total += fight(state->hall_of_fame[i], code, engine); // if first wins, add 1 (worse)
            }
            return 50 * total + engine->size(code);
        }
};

Test * create_tic_off() {
    return new Tic_Off();
}

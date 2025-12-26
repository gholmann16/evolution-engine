#include "competition.hpp"
#define MAX_RUNTIME 1000000

// 1 v 1 tic tac toe
class Tic_Off : public Competition {
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

    public:
        // false = 0 = first wins
        // true = 1 = second wins
        bool fight(const void * first, const void * second, Engine * engine) {
            alignas(256) char board[256] = "         ";
            alignas(256) char output[256];
            if (engine->run(first, board, output, MAX_RUNTIME) == MAX_RUNTIME || place(board, output[0], 'X'))
                return true;
            for (int i = 0; i < 4; i++) {
                if (engine->run(second, board, output, MAX_RUNTIME) == MAX_RUNTIME || place(board, output[0], 'O'))
                    return false;
                if (won(board, 'O'))
                    return true;
                if (engine->run(first, board, output, MAX_RUNTIME) == MAX_RUNTIME || place(board, output[0], 'X'))
                    return true;
                if (won(board, 'X'))
                    return false;
            }
            // if no one won, give it to second, because they didn't have the advantage
            return first > second ? true : false; // if first is larger, second wins
        }
};

Competition * create_tic_off() {
    return new Tic_Off();
}

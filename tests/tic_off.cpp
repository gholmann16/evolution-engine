#define MAX_TIC_OFF 100000

// 1 v 1 tic tac toe
class Tic_Off : public Test {
    private:
        static bool won(char board[256]) {
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

        static bool place(char board[256], unsigned char spot) {
            if (board[spot] == ' ') {
                board[spot] = 'X';
                return false;
            }
            return true;
        }

        static void flip(char board[256]) {
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
        static int fight(Engine * first, Engine * second) {
            alignas(256) char board[256] = "         ";
            alignas(256) char output[256];
            if (first->run(board, output, MAX_TIC_OFF) == MAX_TIC_OFF || place(board, output[0]))
                return -1;
            for (int i = 0; i < 4; i++) {
                flip(board);
                if (second->run(board, output, MAX_TIC_OFF) == MAX_TIC_OFF || place(board, output[0]))
                    return 1;
                if (won(board))
                    return -1;
                flip(board);
                if (first->run(board, output, MAX_TIC_OFF) == MAX_TIC_OFF || place(board, output[0]))
                    return -1;
                if (won(board))
                    return 1;
            }
            return 0;
        }

    public:
        size_t score(const void * code) const override {
            State::engine->load(code);
            // 100 winners of last generation, who don't play cause they already have a score
            size_t total = State::total_famers * 2;
            for (size_t i = 0; i < State::total_famers; i++) {
                State::comp->load(State::hall_of_fame[i]);
                total -= fight(State::engine, State::comp); // if first wins, subtract 1 (better)
                total += fight(State::comp, State::engine); // if first wins, add 1 (worse)
            }
            return 50 * total + State::engine->size(code);
        }
};

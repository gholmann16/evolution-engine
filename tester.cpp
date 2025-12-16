#include "evolver.hpp"
#include "engines/neural_based/neuron.hpp"

Engine * engine = create_network();


void run_scenario(std::string code, char board[256]) {
    char output[256];
    engine->run(code, board, output, 0);
    printf("%c | %c | %c\n", board[0], board[1], board[2]);
    printf("%c | %c | %c\n", board[3], board[4], board[5]);
    printf("%c | %c | %c\n", board[6], board[7], board[8]);
    printf("computer chose: %d\n", output[0]);
}

int main() {
    Test * tester = create_tictactoe();
    Synapse connections[] = {
        {0, 27, 1.0},
        {3, 28, 1.0},
        {6, 29, 1.0},
        {9, 30, 1.0},
        {12, 31, 1.0},
        {15, 32, 1.0},
        {18, 33, 1.0},
        {21, 34, 1.0},
        {24, 35, 1.0},
    };
    struct Program prog = {
        .code = std::string(reinterpret_cast<const char*>(connections), sizeof(connections)),
        .score = 0,
    };
    tester->score(&prog, engine);
    printf("score comes out to %d\n", prog.score);

    // run_scenario(prog.code, "OO       ");

    return 0;
}

#include "evolver.hpp"
#include "engines/neural_based/neuron.hpp"
#include "tests/test.hpp"
#include "competitions/competition.hpp"
#include <iostream>
#include <format>

Engine * engine = create_network();

int main() {
    Test * tester = create_tictactoe();
    Synapse connections[] = {
        {0, 29, 1.0},
        {3, 30, 1.0},
        {6, 31, 1.0},
        {9, 32, 1.0},
        {12, 33, 1.0},
        {15, 34, 1.0},
        {18, 35, 1.0},
        {21, 36, 1.0},
        {24, 37, 1.0},
    };
    struct Program prog = {
        .code = std::string(reinterpret_cast<const char*>(connections), sizeof(connections)),
        .score = 0,
    };
    // struct Program other = {
    //     .code = std::string(),
    //     .score = 0,
    // };
    tester->score(&prog, engine);
    printf("score comes out to %d\n", prog.score);
    std::cout << engine->debug(prog.code);
    // tester->score(&other, engine);
    // printf("score comes out to %d\n", other.score);

    // Competition * competition = create_tic_off();
    // bool win = competition->fight(prog.code, other.code, engine);
    // printf("%s\n", win ? "true" : "false");
    char game[256] = "         ";
    char output[256];
    printf("%c | %c | %c\n", game[0], game[1], game[2]);
    printf("%c | %c | %c\n", game[3], game[4], game[5]);
    printf("%c | %c | %c\n", game[6], game[7], game[8]);

    for (int i = 0; i < 5; i++) {
        engine->run(prog.code, game, output, 0);
        printf("AI chooses: %d\n", output[i]);
        game[output[0]] = 'X';
        printf("%c | %c | %c\n", game[0], game[1], game[2]);
        printf("%c | %c | %c\n", game[3], game[4], game[5]);
        printf("%c | %c | %c\n", game[6], game[7], game[8]);
        printf("User chooses: ");
        int choice;
        std::cin >> choice;
        game[choice] = 'O';
        printf("%c | %c | %c\n", game[0], game[1], game[2]);
        printf("%c | %c | %c\n", game[3], game[4], game[5]);
        printf("%c | %c | %c\n", game[6], game[7], game[8]);
    }
    

    return 0;
}

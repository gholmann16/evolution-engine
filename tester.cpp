#include "evolver.hpp"
#include "engines/neural_based/neuron.hpp"
#include <iostream>
#include <format>

Engine * engine = create_network();

int main() {
    Test * tester = create_tictactoe();
    Synapse connections[] = {
        {0, 27, 1.0},
        {3, 28, 1.0259159},
        {8, 30, 0.44371375},
        {22, 3, -1.6498433},
        {6, 29, 1.0},
        {9, 30, 0.89772147},
        {12, 31, 1.4803133},
        {15, 32, 0.9958158},
        {18, 33, 0.92094046},
        {8, 22, 3.0664287},
        {20, 32, -1.763947},
        {21, 34, 1.0579758},
        {22, 18, 0.17106122},
        {24, 35, 0.9803344},
    };
    struct Program prog = {
        .code = std::string(reinterpret_cast<const char*>(connections), sizeof(connections)),
        .score = 0,
    };
    // tester->score(&prog, engine);
    // printf("score comes out to %d\n", prog.score);

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

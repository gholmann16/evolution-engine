#include "engines/neural_based/brain.hpp"
#include "tests/test.hpp"
#include <iostream>
#include <format>

Engine * engine = create_network();

int main() {
    Test * tester = create_tictactoe();
    struct Brain * brain = new Brain();

    Synapse connections[] = {
        {0, 100, 1.0},
        {3, 103, 1.0},
        {6, 106, 1.0},
        {9, 109, 1.0},
        {12, 112, 1.0},
        {15, 115, 1.0},
        {18, 118, 1.0},
        {21, 121, 1.0},
        {24, 124, 1.0},
        {100, 247, 1.0},
        {103, 248, 1.0},
        {106, 249, 1.0},
        {109, 250, 1.0},
        {112, 251, 1.0},
        {115, 252, 1.0},
        {118, 253, 1.0},
        {121, 254, 1.0},
        {124, 255, 1.0},
    };

    for (Synapse napse : connections) {
        brain->add_synapse(napse);
    }

    struct Program prog = {
        .code = brain,
        .score = 0,
    };

    Synapse connections2[] = {
        {0, 100, 1.0},
        {3, 103, 1.0},
        {6, 106, 1.0},
        {9, 109, 1.0},
        {12, 112, 1.0},
        {15, 115, 1.0},
        {18, 118, 1.0},
        {21, 121, 1.0},
        {24, 124, 1.0},
        {100, 247, 1.0},
        {103, 248, 1.0},
        {106, 249, 1.0},
        {109, 250, 1.0},
        {112, 251, 1.0},
        {115, 252, 1.0},
        {118, 253, 1.0},
        {121, 254, 1.0},
        {124, 255, 1.0},
    };
    struct Brain * brain2 = new Brain();

    for(Synapse new_napse : connections2) {
        brain2->add_synapse(new_napse);        
    }

    tester->score(&prog, engine, NULL);
    // srand(time(NULL));
    printf("score comes out to %d\n", prog.score);
    std::cout << engine->debug(prog.code);
    // std::string new_code;
    // engine->evolve(prog.code, new_code, 250);
    // std::cout << engine->debug(new_code);
    // tester->score(&other, engine);
    // printf("score comes out to %d\n", other.score);

    // Competition * competition = create_tic_off();
    // bool win = competition->fight(prog.code, other.code, engine);
    // printf("%s\n", win ? "true" : "false");
    // char game[256] = "         ";
    // char output[256];
    // printf("%c | %c | %c\n", game[0], game[1], game[2]);
    // printf("%c | %c | %c\n", game[3], game[4], game[5]);
    // printf("%c | %c | %c\n", game[6], game[7], game[8]);

    // for (int i = 0; i < 5; i++) {
    //     engine->run(prog.code, game, output, 0);
    //     printf("AI chooses: %d\n", output[i]);
    //     game[output[0]] = 'X';
    //     printf("%c | %c | %c\n", game[0], game[1], game[2]);
    //     printf("%c | %c | %c\n", game[3], game[4], game[5]);
    //     printf("%c | %c | %c\n", game[6], game[7], game[8]);
    //     printf("User chooses: ");
    //     int choice;
    //     std::cin >> choice;
    //     game[choice] = 'O';
    //     printf("%c | %c | %c\n", game[0], game[1], game[2]);
    //     printf("%c | %c | %c\n", game[3], game[4], game[5]);
    //     printf("%c | %c | %c\n", game[6], game[7], game[8]);
    // }
    

    return 0;
}

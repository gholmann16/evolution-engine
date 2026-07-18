#include "state.hpp"
#include "engines/neural_based/brain.hpp"
#include <iostream>
#include <format>


int main() {
    State::engine = make_engine(0);
    Test * tester = tests[3];
    void * code = State::engine->ancestor_prog();

    // srand(time(NULL));
    printf("score comes out to %d\n", tester->score(code));

    char game[256] = "         ";
    char output[256];
    printf("%c | %c | %c\n", game[0], game[1], game[2]);
    printf("%c | %c | %c\n", game[3], game[4], game[5]);
    printf("%c | %c | %c\n", game[6], game[7], game[8]);

    for (int i = 0; i < 5; i++) {
        // loaded by test
        State::engine->run(game, output, 0);
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

#include <state.hpp>
#include "crc8.cpp"
#include "output.cpp"
#include "tic_off.cpp"
#include "tictactoe.cpp"
#include "tournament_toe.cpp"
#include "add.cpp"

Test * tests[] = {
    new Crc8(),
    new Output(),
    new Tic_Off(),
    new TicTacToe(),
    new Tournament_Toe(),
    new Add(),
};

const char * test_names[] = {
    "Crc8",
    "Output",
    "Tic_Off",
    "TicTacToe",
    "Tournament_Toe",
    "Add",
};

int num_tests = sizeof(tests) / sizeof(Test *);

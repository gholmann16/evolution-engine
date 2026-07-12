#include <state.hpp>
#include "crc8.cpp"
#include "output.cpp"
#include "tic_off.cpp"
#include "tictactoe.cpp"
#include "add.cpp"

Test * tests[] = {
    new Crc8(),
    new Output(),
    new Tic_Off(),
    new TicTacToe(),
    new Add(),
};

const char * test_names[] = {
    "Crc8",
    "Output",
    "Tic_Off",
    "TicTacToe",
    "Add",
};

int num_tests = sizeof(tests) / sizeof(Test *);

#include <state.hpp>
#include "neural_based/network.cpp"
#include "brainfuck_based/brainfuck.cpp"
#include "brainfuck_based/assembler.cpp"

Engine * engines[] {
    new Network(),
    new Brainfuck(),
    new JitFuck(),
};

const char * engine_names[] {
    "Network",
    "Brainfuck",
    "JitFuck",
};

int num_engines = 3;

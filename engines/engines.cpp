#include <state.hpp>
#include "neural_based/network.cpp"
#include "neural_based/racehorse.cpp"
#include "brainfuck_based/brainfuck.cpp"
#include "brainfuck_based/assembler.cpp"

Engine * engines[] {
    new Network(),
    new Brainfuck(),
    new JitFuck(),
    new Racehorse(),
};

Engine * competitors[] {
    new Network(),
    new Brainfuck(),
    new JitFuck(),
    new Racehorse(),
};

const char * engine_names[] {
    "Network",
    "Brainfuck",
    "JitFuck",
    "Racehorse"
};

int num_engines = sizeof(engines) / sizeof(Engine *);

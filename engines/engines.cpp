#include <state.hpp>
#include "neural_based/network.cpp"
#include "neural_based/racehorse.cpp"
#include "brainfuck_based/brainfuck.cpp"
#include "brainfuck_based/assembler.cpp"
#include "brainfuck_based/skipfuck.cpp"

Engine * engines[] {
    new Network(),
    new Brainfuck(),
    new JitFuck(),
    new Racehorse(),
    new Skipfuck(),
};

Engine * competitors[] {
    new Network(),
    new Brainfuck(),
    new JitFuck(),
    new Racehorse(),
    new Skipfuck(),
};

const char * engine_names[] {
    "Network",
    "Brainfuck",
    "JitFuck",
    "Racehorse",
    "Skipfuck",
};

int num_engines = sizeof(engines) / sizeof(Engine *);

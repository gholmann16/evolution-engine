#include <state.hpp>
#include "neural_based/network.cpp"
#include "neural_based/racehorse.cpp"
#include "brainfuck_based/brainfuck.cpp"
#include "brainfuck_based/assembler.cpp"
#include "brainfuck_based/skipfuck.cpp"

const char * engine_names[] {
    "Network",
    "Brainfuck",
    "JitFuck",
    "Racehorse",
    "Skipfuck",
};

// One constructor per engine, in the same order as engine_names[] -- the
// only place that needs to know every concrete engine type. No standing
// instances: make_engine()/clone_engine() build exactly what's asked for.
static Engine * (*raw_factories[])() {
    [] () -> Engine * { return new Network(); },
    [] () -> Engine * { return new Brainfuck(); },
    [] () -> Engine * { return new JitFuck(); },
    [] () -> Engine * { return new Racehorse(); },
    [] () -> Engine * { return new Skipfuck(); },
};

int num_engines = sizeof(raw_factories) / sizeof(raw_factories[0]);

Engine * make_engine(int id) {
    Engine * e = raw_factories[id]();
    e->factory_id = id;
    return e;
}

Engine * clone_engine(const Engine * e) {
    return make_engine(e->factory_id);
}

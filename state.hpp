#pragma once
#define VERBOSE true

struct Program {
    void * code;
    unsigned long long score;
};

struct State {
    long int seed;
    int runs;
    int total_winners;
    int def_rand;
    int repetitions;
    struct Program * children; // struct Program * total_winners^2
};

struct State def_state();
struct State load(char * file);

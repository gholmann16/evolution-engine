#pragma once
#define VERBOSE true// struct Program * total_winners^2

struct Program {
    void * code;
    unsigned long long score;
};

struct State {
    struct Program * children;
    unsigned long long seed;
    int runs;
    int def_rand;
    int repetitions;
    int total_creatures;
};

struct State def_state();
struct State load(char * file);

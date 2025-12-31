#pragma once
#define VERBOSE true// struct Program * total_winners^2
#define HALL_OF_FAMERS 100

struct Program {
    void * code;
    unsigned long long score;
};

struct State {
    struct Program * children;
    void * hall_of_fame[HALL_OF_FAMERS];
    unsigned long long seed;
    int runs;
    int def_rand;
    int repetitions;
    int total_creatures;
};

struct State def_state();
struct State load(char * file);

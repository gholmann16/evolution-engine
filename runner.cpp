#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include "engines/engine.hpp"
#include "tests/test.hpp"
#include <iostream>
#include "crcbf.h"
#include "state.hpp"
#include <algorithm>
#include <random>
#include <fstream>

bool compare_ratings(const Program& first, const Program &second) {
    return first.score < second.score;
}

struct Program last = {
    .code = NULL,
    .score = 0,
};

// Returns true if repeat
bool generation(struct State state, Test * tester, Engine * engine) {
    // Determinism
    double before = clock();
    srand(state.seed + state.runs);

    // Evolve from winning pool
    for (int winner = 0; winner < state.total_winners; winner++) {
        // Keep the winner around so you never regress (agamogenesis) and reset
        // state.children[winner].score = 0;

        // Other grandchildren slightly modified, reset by evolve()
        for (int grandchild = 1; grandchild < state.total_winners; grandchild++) {
            engine->evolve(state.children[winner].code, state.children[grandchild * state.total_winners + winner].code, state.def_rand - state.repetitions);
            state.children[grandchild * state.total_winners + winner].score = 0;
        }
    }
    double after = clock();
    if (VERBOSE)
        printf("Time generating: %lf\n", (after - before) / CLOCKS_PER_SEC);

    before = clock();
    // no reason to re-test the winners since we're not changing them
    for (int i = state.total_winners; i < state.total_winners * state.total_winners; i++) {
        tester->score(&(state.children[i]), engine, &state);
    }
    after = clock();

    if (VERBOSE)
        printf("Time scoring: %lf\n", (after - before) / CLOCKS_PER_SEC);


    before = clock();
    std::sort(state.children, state.children + (state.total_winners * state.total_winners), compare_ratings);

    // for (int i = 0; i < state.total_winners * state.total_winners; i++) {
    //     printf("Winner %d score is %ld\n", i, state.children[i].score);
    // }
    // Get winning pool, shoot for diversity
    int found = 1;
    for (int candidate = 1; candidate < state.total_winners * state.total_winners && found < state.total_winners; candidate++) {
        /* 
        * If we haven't found all winners, and this one is not identical to the previous winner
        * Can still have duplicates probably but not all duplicates, at least some genetic variety
        * If they're all the same hypothetically, then some left over dna from last generation would stay
        * Conveniently, the worse dna of the batch because the winners will stay winners
        * Frees everything we need, since parent and children share a pool
        */
        if (engine->equal(state.children[candidate].code, state.children[found - 1].code) == false)
            state.children[found++] = state.children[candidate];
    }
    while (found < state.total_winners)
        state.children[found++] = state.children[0];

    after = clock();
    if (VERBOSE)
        printf("Time sorting: %lf\n", (after - before) / CLOCKS_PER_SEC);

    // repeats. must be deterministic or score means nothing
    if (state.children[0].score == last.score && engine->equal(state.children[0].code, last.code))
        return true;

    last = state.children[0];
    return false;
}

#define EXECUTIONS 10000000

// Returns false = end program
bool cli_interpret(struct State state, Engine * engine) {
    std::cout << engine->debug(state.children[0].code) << std::endl;

    static double last = 0;
    static double average = 0;
    double time_taken = (clock() - last) / CLOCKS_PER_SEC;
    last = clock();
    average = (average * (state.runs - 1) + time_taken) / state.runs;
    printf("Winner of generation %d won with a score of %llu, size %ld, (%d previously wins) (seed is %ld). Time used %lf (average is %lf).\n", 
        state.runs, state.children[0].score, engine->size(state.children[0].code), state.repetitions, state.seed,
        time_taken, average
    );

    if (state.children[0].score == 0) {
        puts("Solved");
        return false;
    }
    else if (state.def_rand - state.repetitions == 50) {
        puts("I give up");
        return false;
    }

    return true;
}

static volatile bool running = true;

void quit(int sig) {
    running = false;
}

int runner(struct State state) {
    signal(SIGINT, quit);
    Test * tester = create_tic_off();
    Engine * engine = create_network();
    // Set the default, fill whole thing cause why not
    last.code = engine->ancestor_prog();
    for(int ancestor = 0; ancestor < state.total_winners * state.total_winners; ancestor++) {
        state.children[ancestor].code = engine->ancestor_prog();
        state.children[ancestor].score = -1;
    }

    std::ofstream file("data.txt");
    file << "# X Y\n";

    while (running && state.runs < EXECUTIONS) {
        if(generation(state, tester, engine))
            state.repetitions++;
        else
            state.repetitions = 0;
        // brutality(state, competition, engine);

        state.runs++;
        // tester->score(&state.children[0], engine);
        file << state.runs << " " << state.children[0].score << "\n";
        if (cli_interpret(state, engine) == false)
            break;
    }
    file.close();

    // if (running == false)
        // save(state);

    delete[] state.children;
    return 0;
}

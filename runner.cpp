#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include "engines/engine.hpp"
#include "evolvers/evolver.hpp"
#include "tests/test.hpp"
#include <iostream>
#include "state.hpp"
#include <fstream>
#include <functional>
#include <cstring>

void * last_code = NULL;

#define EXECUTIONS 10000000
#define NUM_WIN 100
#define DEFAULT_RANDOMNESS 250

// Returns false = end program
bool cli_interpret(struct State& state, Engine * engine) {
    // repeats. must be deterministic or score means nothing
    bool repeat = false;
    for (int i = 0; i < HALL_OF_FAMERS; i++) {
        if (engine->equal(state.children[0].code, state.hall_of_fame[i])) {
            repeat = true;
            break;
        }
    }
    if (repeat)
        state.repetitions++;
    else {
        int last = HALL_OF_FAMERS - 1;
        void * save = state.hall_of_fame[last];
        std::memmove(&state.hall_of_fame[1], &state.hall_of_fame, last * sizeof(void *));
        state.hall_of_fame[0] = save;
        engine->copy_into(state.children[0].code, state.hall_of_fame[0]);
        state.repetitions = 0;
    }

    static double last = 0;
    static double average = 0;
    double time_taken = (clock() - last) / CLOCKS_PER_SEC;
    last = clock();
    average = (average * (state.runs - 1) + time_taken) / state.runs;
    std::cout << engine->debug(state.children[0].code) << std::endl;
    printf("Winner of generation %d won with a score of %llu, size %ld, (%d previously wins) (seed is %lld). Time used %lf (average is %lf).\n", 
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
    else
        return true;
}

static volatile bool running = true;

void quit(int sig) {
    running = false;
}

void time_func(std::function<void(Test *, Engine *, const struct State)> fn, Test * tester, Engine * engine, const struct State state, const char * line) {
    double before = clock();
    fn(tester, engine, state);
    double after = clock();
    if (VERBOSE) {
        printf("Time %s: %lf\n", line, (after - before) / CLOCKS_PER_SEC);
    }
}

int runner() {
    struct State state = {
        .children = new Program[NUM_WIN * NUM_WIN],
        .seed = (long long unsigned int) time(NULL),
        .def_rand = DEFAULT_RANDOMNESS,
        .total_creatures = NUM_WIN * NUM_WIN,
    };

    signal(SIGINT, quit);
    // Test * tester = create_tic_off();
    Test * tester = create_tictactoe();
    Engine * engine = create_network();
    // Set the default, fill whole thing cause why not
    for(int ancestor = 0; ancestor < state.total_creatures; ancestor++) {
        state.children[ancestor].code = engine->ancestor_prog();
        state.children[ancestor].score = -1;
    }
    for (int famer = 0; famer < HALL_OF_FAMERS; famer++)
        state.hall_of_fame[famer] = engine->ancestor_prog();

    Evolver * evolver = create_squarelite(tester, engine, state);

    std::ofstream file("data.txt");
    file << "# X Y Z\n";

    int percentile[] = {100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 80, 70, 60, 50, 40, 30, 20, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    for (int& mod : percentile) {
        mod *= (state.total_creatures - 1);
        mod /= 100;
    }

    while (running && state.runs < EXECUTIONS) {
        // Determinism
        srand(state.seed + state.runs);

        time_func([&](Test * tester, Engine * engine, const struct State state) { return evolver->evolve(tester, engine, state); }, tester, engine, state, "generating");
        time_func([&](Test * tester, Engine * engine, const struct State state) { return evolver->score_all(tester, engine, state); }, tester, engine, state, "generating");
        time_func([&](Test * tester, Engine * engine, const struct State state) { return evolver->sort(tester, engine, state); }, tester, engine, state, "generating");

        state.runs++;
        file << state.runs;
        for (int x : percentile)
            file << " " << state.children[x].score;
        file << "\n";
        if (cli_interpret(state, engine) == false)
            break;
    }
    file.close();

    // if (running == false)
        // save(state);

    delete[] state.children;
    return 0;
}

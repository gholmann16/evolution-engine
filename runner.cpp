#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <state.hpp>
#include <fstream>
#include <functional>
#include <cstring>

void * last_code = NULL;

#define EXECUTIONS 100000000

// Returns false = end program
bool cli_interpret() {
    bool repeat = false;
    for (size_t i = 0; i < State::total_famers; i++) {
        if (State::engine->equal(State::children[0].code, State::hall_of_fame[i])) {
            repeat = true;
            break;
        }
    }
    if (repeat)
        State::repetitions++;
    else if (State::total_famers) {
        int last = State::total_famers - 1;
        void * save = State::hall_of_fame[last];
        std::memmove(State::hall_of_fame + 1, State::hall_of_fame, last * sizeof(void *));
        State::hall_of_fame[0] = save;
        State::engine->copy_into(State::children[0].code, State::hall_of_fame[0]);
        State::repetitions = 0;
    }

    static double last = 0;
    static double average = 0;
    double time_taken = (clock() - last) / CLOCKS_PER_SEC;
    last = clock();
    average = (average * (State::runs - 1) + time_taken) / State::runs;
    std::cout << State::engine->debug(State::children[0].code) << std::endl;
    printf("Winner of generation %zu won with a score of %zu, size %zu, (%zu previously wins) (seed is %zu). Time used %lf (average is %lf).\n", 
        State::runs, State::children[0].score, State::engine->size(State::children[0].code), State::repetitions, State::seed, time_taken, average
    );

    if (State::children[0].score == 0) {
        puts("Solved");
        return false;
    }
    else if (State::def_rand - State::repetitions == 50) {
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

void time_func(void(Evolver::*fn)() const, const char * line) {
    double before = clock();
    (State::evolver->*fn)();
    double after = clock();
    if (State::verbose) {
        printf("Time %s: %lf\n", line, (after - before) / CLOCKS_PER_SEC);
    }
}

int runner() {
    signal(SIGINT, quit);

    int percentile[] = {100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 80, 70, 60, 50, 40, 30, 20, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    for (int& mod : percentile) {
        mod *= (State::total_creatures - 1);
        mod /= 100;
    }

    // Clear past data
    std::ofstream ofs(State::output, std::ofstream::out | std::ofstream::trunc);
    ofs.close();

    while (running && State::runs < EXECUTIONS) {
        // Determinism
        srand(State::seed + State::runs);

        time_func(&Evolver::evolve, "generating");
        time_func(&Evolver::score_all, "scoring");
        time_func(&Evolver::sort, "sorting");

        std::ofstream file(State::output, std::ios::app);

        State::runs++;
        file << State::runs;
        for (int x : percentile) {
            // state.children[x].score = absolute->score(state.children[x].code, engine, &state);
            file << " " << State::children[x].score;
        }
        file << "\n";
        file.close();

        if (cli_interpret() == false)
            break;
    }

    return 0;
}

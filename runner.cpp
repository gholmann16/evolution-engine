#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <evolver.hpp>
#include <iostream>

#define EXECUTIONS 10000000
#define EXECUTIONS 10000000

// Returns false = end program
bool cli_interpret(struct State state) {
    static double last = 0;
    static double average = 0;
    double time_taken = (clock() - last) / CLOCKS_PER_SEC;
    last = clock();
    average = (average * (state.runs - 1) + time_taken) / state.runs;
    printf("Winner of generation %d won with a score of %ld, size %ld, (%d previously wins) (seed is %ld). Time used %lf (average is %lf).\n", 
        state.runs, state.children[0].score, state.children[0].code.size(), state.repetitions, state.seed,
        time_taken, average
    );
    std::cout << state.children[0].code << std::endl;

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

// void save(struct State state) {
//     FILE * out = fopen("save.bin", "wb");
//     fwrite(&state, sizeof(struct State), 1, out);
//     fwrite(state.children, sizeof(struct Program), state.total_winners, out);
//     fclose(out);
// }

struct State load(char * file) {
    return def_state();
}

// struct State load(char * file) {
//     FILE * in = fopen(file, "rb");
//     struct State revived;
//     fread(&revived, sizeof(struct State), 1, in);
//     revived.parent = (struct Program *) malloc(sizeof(struct Program) * revived.total_winners);
//     revived.children = (struct Program *) malloc(sizeof(struct Program) * revived.total_winners * revived.total_winners);
//     fread(revived.parent, sizeof(struct Program), revived.total_winners, in);
//     fclose(in);
//     return revived;
// }

static volatile bool running = true;

void quit(int sig) {
    running = false;
}

int runner(struct State state) {
    signal(SIGINT, quit);
    Test * tester = create_output();
    Engine * engine = create_jitfuck("+");
    // Set the default
    for(int ancestor = 0; ancestor < state.total_winners; ancestor++)
        state.children[ancestor].code = engine->ancestor_prog();

    while (running && state.runs < EXECUTIONS) {
        bool repeat = generation(state, tester, engine);
        state.runs++;
        if (cli_interpret(state) == false)
            break;
        state.repetitions = repeat*state.repetitions + repeat; // Add 1 if it repeated
    }

    // if (running == false)
        // save(state);

    free(state.children);
    return 0;
}
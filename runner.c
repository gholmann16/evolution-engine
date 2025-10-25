#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <evolver.h>
#include <time.h>
#define EXECUTIONS 10000000

// Returns false = end program
bool cli_interpret(struct State state) {
    static double last = 0;
    printf("Winner of generation %d won with a score of %ld, size %ld, runtime %ld (%d previously wins) (seed is %d). Time used: %lf:\n", 
        state.runs, state.children[0].score, state.children[0].size, state.children[0].runtime, state.repetitions, state.seed, (clock() - last) / CLOCKS_PER_SEC);
    last = clock();
    write(1, state.children[0].code, state.children[0].size);
    puts("");

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

void save(struct State state) {
    FILE * out = fopen("save.bin", "wb");
    fwrite(&state, sizeof(struct State), 1, out);
    fwrite(state.parent, sizeof(struct Program), state.total_winners, out);
    fclose(out);
}

struct State load(char * file) {
    FILE * in = fopen(file, "rb");
    struct State revived;
    fread(&revived, sizeof(struct State), 1, in);
    revived.parent = malloc(sizeof(struct Program) * revived.total_winners);
    revived.children = malloc(sizeof(struct Program) * revived.total_winners * revived.total_winners);
    fread(revived.parent, sizeof(struct Program), revived.total_winners, in);
    fclose(in);
    return revived;
}

static volatile bool running = true;

void quit(int sig) {
    running = false;
}

int runner(struct State state) {
    init_env();
    signal(SIGINT, quit);
    // Set the default
    for(int ancestor = 0; ancestor < state.total_winners; ancestor++)
        state.parent[ancestor] = ancestor_prog();

    while (running && state.runs < EXECUTIONS) {
        bool repeat = generation(state);
        state.runs += EXECUTIONS * !cli_interpret(state);
        state.repetitions = repeat*state.repetitions + repeat; // Add 1 if it repeated
        state.runs++;
    }

    if (running == false)
        save(state);

    free(state.parent);
    free(state.children);
    free_env();
    return 0;
}
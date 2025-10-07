#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <evolver.h>
#include <crc8.h>

#define EXECUTIONS 10
#define NUM_WIN 10
#define DEFAULT_RANDOMNESS 10050

int compare_ratings(const void * first, const void * second) {
    // First minus second because we now want to sort in ascending order
    return ((struct Program *) first)->score - ((struct Program *) second)->score; 
}

bool compare_code(struct Program first, struct Program second) {
    if (first.size != second.size)
        return true;
    for (int i = 0; i < first.size; i++)
        if (first.code[i] != second.code[i]) // Compare as strings rather than voids
            return true;
    return false;
}

void fill_string(char input[256]) {
    for (int i = 0; i < 255; i++) {
        input[i] = rand() % 256;
    }
    input[255] = 0;
}

void score(struct Program * program, char inputs[3][256], char outputs[3]) {
    int sc = 0;
    program->runtime = 0;
    char output[256];
    static int runnum = -1;
    runnum++;

    for (int i = 0; i < 3; i++) {
        run(program, inputs[i], output);

        if (program->runtime == MAX_RUNTIME) {
            program->score = WORST;
            return;
        }

        sc += abs(outputs[i] - output[0]);
    }
    if (sc)
        program->score = WORST;
        // program->score = sc * 50 + program->runtime;// + program->size * 2;
    else
        program->score = program->runtime + program->size; // If no difference it's golden
    printf("Contestor %d has score of %ld\n", runnum, program->score);
}

// Returns true if repeat
bool generation(struct State state) {
    // Determinism
    srand(state.seed + state.runs);

    // Evolve from winning pool
    for (int winner = 0; winner < state.total_winners; winner++) {
        // Keep the winner around so you never regress (agamogenesis)
        state.children[winner * state.total_winners] = state.parent[winner];
        for (int grandchild = 1; grandchild < state.total_winners; grandchild++)
            state.children[winner * state.total_winners + grandchild] = evolve(state.parent[winner], state.def_rand - state.repetitions);
    }

    char inputs[3][256];
    char outputs[3];
    char tmp[256];
    for (int which = 0; which < 3; which++) {
        fill_string(inputs[which]);
        memcpy(tmp, inputs[which], 256);
        outputs[which] = crc8(tmp);
    }

    for (int i = 0; i < state.total_winners * state.total_winners; i++)
        score(&(state.children[i]), inputs, outputs);

    qsort(state.children, state.total_winners * state.total_winners, sizeof(struct Program), compare_ratings);
    bool rep = (state.children[0].score == state.parent[0].score) ? true : false;

    printf("state of generation:\n1: %ld\n2: %ld\n3: %ld\n4: %ld\n5: %ld\n", state.children[0].score, state.children[1].score, state.children[2].score, state.children[3].score, state.children[4].score);
    // Get winning pool, shoot for diversity
    state.parent[0] = state.children[0];
    int found = 1;
    for (int candidate = 1; candidate < state.total_winners * state.total_winners; candidate++) {
        /* 
            * If we haven't found all winners, and this one is not identical to the previous winner
            * Can still have duplicates probably but not all duplicates, at least some genetic variety
            * If they're all the same hypothetically, then some left over dna from last generation would stay
            * Conveniently, the worse dna of the batch because the winners will stay winners
            * Frees everything we need, since parent and children share a pool
            */
        if (found < state.total_winners && compare_code(state.children[candidate], state.parent[found - 1]))
            state.parent[found++] = state.children[candidate];
    }
    while (found < state.total_winners)
        state.parent[found++] = state.parent[0];

    return rep;
}

// Returns false = end program
bool cli_interpret(struct State state) {
    printf("Winner of generation %d won with a score of %ld, size %ld, runtime %ld! (%d previously wins) (seed is %d):\n", 
        state.runs, state.children[0].score, state.children[0].size, state.children[0].runtime, state.repetitions, state.seed);
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

struct State def_state() {
    return (struct State) {
        .seed = time(NULL),
        .total_winners = NUM_WIN,
        .def_rand = DEFAULT_RANDOMNESS,
        .parent = malloc(sizeof(struct Program) * NUM_WIN),
        .children = malloc(sizeof(struct Program) * NUM_WIN * NUM_WIN),
    };
}

static volatile bool running = true;

void quit(int sig) {
    running = false;
}

int evolver(struct State state) {
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
    return 0;
}

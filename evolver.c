#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <evolver.h>
#include <crc8.h>

#define NUM_WIN 3
#define DEFAULT_RANDOMNESS 10050

int compare_ratings(const void * first, const void * second) {
    // First minus second because we now want to sort in ascending order
    long long diff = ((struct Program *) first)->score - ((struct Program *) second)->score;
    return (int)((diff > 0) - (diff < 0)); // avoid overflow
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

    for (int i = 0; i < state.total_winners * state.total_winners; i++) {
        printf("Winner %d score is %ld\n", i, state.children[i].score);
    }
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

struct State def_state() {
    return (struct State) {
        .seed = 50,
        .total_winners = NUM_WIN,
        .def_rand = DEFAULT_RANDOMNESS,
        .parent = malloc(sizeof(struct Program) * NUM_WIN),
        .children = malloc(sizeof(struct Program) * NUM_WIN * NUM_WIN),
    };
}

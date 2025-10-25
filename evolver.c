#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <evolver.h>

#define NUM_WIN 100
#define DEFAULT_RANDOMNESS 1050

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

void score(struct Program * prog, char * inputs, char * expect) {
    alignas(256) char output[256] = {0};

    for (int test = 0; test < reps(); test++) {
        run(prog, inputs + (test * 256), output);
        if (prog->runtime == max_runtime())
            break;

        if (read_all())
            for (int ch = 0; ch < 256; ch++) {
                prog->score += abs(output[ch] - expect[test * 256 + ch]);
            }
        else {
            int diff = strlen(expect + (test * 256)) - strlen(output);
            int max = (diff < 0) ? strlen(expect + (test * 256)) : strlen(output);
            prog->score += 255 * abs(diff);
            for (int ch = 0; ch < max; ch++) {
                prog->score += abs(expect[test * 256 + ch] - output[ch]);
            }
        }
    }

    if (exact()) {
        if (prog->score || prog->runtime == max_runtime())
            prog->score = max_runtime();
        else
            prog->score = prog->runtime + prog->size * 5;
    }
    else { // Non exact
        if (prog->score || prog->runtime == max_runtime())
            prog->score = prog->score * 50 + prog->runtime + prog->size * 5;
        else
            prog->score = 0;
    }
}

// Returns true if repeat
bool generation(struct State state) {
    // Determinism
    srand(state.seed + state.runs);

    // Evolve from winning pool
    for (int winner = 0; winner < state.total_winners; winner++) {
        // Keep the winner around so you never regress (agamogenesis) and reset
        state.children[winner * state.total_winners] = state.parent[winner];
        state.children[winner * state.total_winners].score = 0;
        state.children[winner * state.total_winners].runtime = 0;

        // Other grandchildren slightly modified, reset by evolve()
        for (int grandchild = 1; grandchild < state.total_winners; grandchild++)
            state.children[winner * state.total_winners + grandchild] = evolve(state.parent[winner], state.def_rand - state.repetitions);
    }

    alignas(256) char * inputs = malloc(reps() * 256);
    char * expect = malloc(reps() * 256);
    for (int test = 0; test < reps(); test++) {
        answer(inputs + (test * 256), expect + (test * 256));
    }
    for (int i = 0; i < state.total_winners * state.total_winners; i++) {
        score(&(state.children[i]), inputs, expect);
    }
    free(inputs);
    free(expect);

    qsort(state.children, state.total_winners * state.total_winners, sizeof(struct Program), compare_ratings);
    bool rep = !compare_code(state.children[0], state.parent[0]);

    // for (int i = 0; i < state.total_winners * state.total_winners; i++) {
    //     printf("Winner %d score is %ld\n", i, state.children[i].score);
    // }
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
        .seed = time(NULL),
        .total_winners = NUM_WIN,
        .def_rand = DEFAULT_RANDOMNESS,
        .parent = malloc(sizeof(struct Program) * NUM_WIN),
        .children = malloc(sizeof(struct Program) * NUM_WIN * NUM_WIN),
    };
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "brainfuck.h"
#include "generator.h"
#include "game.h"

#define EXECUTIONS 100000
#define WORST 50000

#define NUM_WIN 100
#define NUM_CHILD NUM_WIN * NUM_WIN
#define NUM_GRAND NUM_CHILD / NUM_WIN

#define DEFAULT_RANDOMNESS 550
int randomness = DEFAULT_RANDOMNESS;

struct Program {
    char * brainfuck;
    int score;
};

int compare_ratings(const void * first, const void * second) {
    // First minus second because we now want to sort in ascending order
    return ((struct Program *) first)->score - ((struct Program *) second)->score; 
}

int score(char * code) {
    char * question = "2x^3";
    char * response = run(code, question, strlen(question));
    char * word = "6x^2";

    if (response == NULL)
        return WORST;

    int diff = strlen(word) - strlen(response);
    int sc = abs(diff) * 255;
    int compare = (diff > 0) ? strlen(response) : strlen(word);
    for (int i = 0; i < compare; i++)
        sc += abs(word[i] - response[i]);

    sc *= 50;
    free(response);
    return sc + strlen(code);
}

int main() {

    srand(time(NULL));
    struct Program children[NUM_CHILD] = {0};
    struct Program parent[NUM_WIN] = {0};

    // Set the default
    for(int ancestor = 0; ancestor < NUM_WIN; ancestor++)
        parent[ancestor].brainfuck = strdup(".>.>.>.>.");

    int best = WORST;
    int times = 0;

    for(int runs = 0; runs < EXECUTIONS; runs++) {
        // Evolve from winning pool
        for (int winner = 0; winner < NUM_WIN; winner++) {
            // Keep the winner around so you never regress (agamogenesis)
            children[winner * NUM_GRAND].brainfuck = parent[winner].brainfuck;
            for (int grandchild = 1; grandchild < NUM_GRAND; grandchild++)
                children[winner * NUM_GRAND + grandchild].brainfuck = evolve(parent[winner].brainfuck);
        }
        for (int i = 0; i < NUM_CHILD; i++) {
            children[i].score = score(children[i].brainfuck);
        }

        qsort(children, NUM_CHILD, sizeof(struct Program), compare_ratings);

        if (children[0].score == best)
            times++;
        else {
            times = 0;
            best = children[0].score;
        }

        if (children[0].brainfuck)
            printf("Winner of generation %d with a score of %d is %s (won %d times)\n", 
                runs, children[0].score, children[0].brainfuck, times);
        if (children[0].score == strlen(children[0].brainfuck)) {
            puts("Solved");
            break;
        }

        randomness = DEFAULT_RANDOMNESS - times;
        if (randomness == 50) {
            puts("I give up");
            break;
        }

        // Get winning pool, shoot for diversity
        parent[0].brainfuck = children[0].brainfuck;
        int found = 1;
        for (int candidate = 1; candidate < NUM_CHILD; candidate++) {
            /* 
             * If we haven't found all winners, and this one is not identical to the previous winner
             * Can still have duplicates probably but not all duplicates, at least some genetic variety
             * If they're all the same hypothetically, then some left over dna from last generation would stay
             * Conveniently, the worse dna of the batch because the winners will stay winners
             * Frees everything we need, since parent and children share a pool
             */
            if (found != NUM_WIN && strcmp(children[candidate].brainfuck, parent[found - 1].brainfuck) != 0)
                parent[found++].brainfuck = children[candidate].brainfuck;
            else
                free(children[candidate].brainfuck);
        }
    }

    for (int to_free = 0; to_free < NUM_WIN; to_free++)
        free(parent[to_free].brainfuck);

    return 0;
}
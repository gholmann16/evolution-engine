#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "game.h"
#include "generator.h"

struct Program {
    char * brainfuck;
    size_t score;
};

int compare_ratings(const void * first, const void * second) {
    // Second minus fist because we want to sort in descending order
    return ((struct Program *) second)->score - ((struct Program *) first)->score; 
}

int main() {

    srand(time(NULL));
    struct Program children[NUM_CHILD] = {0};

    for(int runs = 0; runs < EXECUTIONS; runs++) {
        // Go backwards for evolve in place
        for (int winner = NUM_WIN - 1; winner >= 0; winner--) {
            // Also needs to go backwards, otherwise the last child (absolute winner) would overwrite itself while evolving
            for (int grandchild = NUM_GRAND - 1; grandchild >= 0; grandchild--) {
                char * dna = evolve(children[winner].brainfuck);
                children[winner * NUM_GRAND + grandchild].brainfuck = dna;
            }
        }
        for (int i = 0; i < NUM_CHILD; i++) {
            children[i].score = game(children[i].brainfuck);
        }

        qsort(children, NUM_CHILD, sizeof(struct Program), compare_ratings);
        if (children[0].brainfuck)
            puts(children[0].brainfuck);
    }
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "brainfuck.h"
#include "generator.h"

#define EXECUTIONS 10000
#define MAGIC_NUMBER 100

#define NUM_CHILD 100
#define NUM_WIN 10
#define NUM_GRAND (NUM_CHILD / NUM_WIN)

struct Program {
    char * brainfuck;
    int score;
};

int compare_ratings(const void * first, const void * second) {
    // First minus second because we now want to sort in ascending order
    return ((struct Program *) first)->score - ((struct Program *) second)->score; 
}

int score(char * code) {
    return abs(MAGIC_NUMBER - run(code, NULL, 0))*10 + strlen(code);
}

int main() {

    srand(time(NULL));
    struct Program children[NUM_CHILD] = {0};

    for(int runs = 0; runs < EXECUTIONS; runs++) {
        // Go backwards for evolve in place
        for (int winner = NUM_WIN - 1; winner >= 0; winner--) {
            // Also needs to go backwards, otherwise the last child (absolute winner) would overwrite itself while evolving
            for (int grandchild = NUM_GRAND - 1; grandchild >= 0; grandchild--) {
                // printf("child %d evolved with code %s\n", winner, children[winner].brainfuck);
                char * dna = evolve(children[winner].brainfuck);
                free(children[winner * NUM_GRAND + grandchild].brainfuck);
                children[winner * NUM_GRAND + grandchild].brainfuck = dna;
                // printf("child %d assigned to %s\n", winner * NUM_GRAND + grandchild, dna);
            }
        }
        for (int i = 0; i < NUM_CHILD; i++) {
            children[i].score = score(children[i].brainfuck);
        }

        qsort(children, NUM_CHILD, sizeof(struct Program), compare_ratings);
        if (children[0].brainfuck)
            printf("Winner of generation %d with a score of %d is %s\n", runs, children[0].score, children[0].brainfuck);
        if (children[0].score == 0)
            return 0;
    }
    return 0;
}
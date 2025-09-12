#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "brainfuck.h"
#include "generator.h"
#include "game.h"

#define EXECUTIONS 1000
#define MAGIC_NUMBER 100

#define NUM_WIN 100
#define NUM_CHILD NUM_WIN * NUM_WIN
#define NUM_GRAND NUM_CHILD / NUM_WIN

struct Program {
    char * brainfuck;
    int score;
};

int compare_ratings(const void * first, const void * second) {
    // First minus second because we now want to sort in ascending order
    return ((struct Program *) first)->score - ((struct Program *) second)->score; 
}

int score(char * code) {
    char * response = run(code, "Hello", 5);
    char * word = "Hi there";

    if (response == NULL)
        return 50000;

    int diff = strlen(word) - strlen(response);
    int sc = abs(diff) * 255;
    int compare = (diff > 0) ? strlen(response) : strlen(word);
    for (int i = 0; i < compare; i++) {
        sc += abs(word[i] - response[i]);
        if (word[i] - response[i] == 0)
            sc - 10;
    }

    sc *= 10;
    free(response);
    return sc + strlen(code);
}

int main() {

    srand(time(NULL));
    struct Program children[NUM_CHILD] = {0};
    // Set the default
    for(int ancestor = 0; ancestor < NUM_WIN; ancestor++)
        children[ancestor].brainfuck = strdup(".>.>.>.>.");

    for(int runs = 0; runs < EXECUTIONS; runs++) {
        // Go backwards for evolve in place
        for (int winner = NUM_WIN - 1; winner >= 0; winner--) {
            // Also needs to go backwards, otherwise the last child (absolute winner) would overwrite itself while evolving
            for (int grandchild = NUM_GRAND - 1; grandchild > 0; grandchild--) {
                char * dna = evolve(children[winner].brainfuck);
                free(children[winner * NUM_GRAND + grandchild].brainfuck);
                children[winner * NUM_GRAND + grandchild].brainfuck = dna;
            }
            // Keep the winner around so you never regress
            char * agamogenesis = strdup(children[winner].brainfuck);
            free(children[winner * NUM_GRAND].brainfuck);
            children[winner * NUM_GRAND].brainfuck = agamogenesis;
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

    for (int to_free = 0; to_free < NUM_CHILD; to_free++)
        free(children[to_free].brainfuck);

    return 0;
}
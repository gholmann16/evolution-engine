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
    char * response = run(code, "Hello", 5);
    char * word = "Hi there";

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
    // Set the default
    for(int ancestor = 0; ancestor < NUM_WIN; ancestor++)
        children[ancestor].brainfuck = strdup(".>.>.>.>.");

    int best = WORST;
    int times = 0;

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
    }

    for (int to_free = 0; to_free < NUM_CHILD; to_free++)
        free(children[to_free].brainfuck);

    return 0;
}
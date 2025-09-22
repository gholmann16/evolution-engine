#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <main.h>

#define EXECUTIONS 100000000

#define NUM_WIN 100
#define NUM_CHILD NUM_WIN * NUM_WIN
#define NUM_GRAND NUM_CHILD / NUM_WIN

#define DEFAULT_RANDOMNESS 1050

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

char * inputs[256] = {"hello brainfuck you got this", "another line of text long", "3rd input and comparing to hash"};
char * outputs[] = { "e34c8297bcead2c833764455620e3d395ef346d66aef4dda3c704f55ed1dc29c", 
                    "4ca9a0e789bbb2aaa5052f61d0f4fbeb99e21ffba1f9926c8a240447d4c50aa3",
                    "d52db1e96610465aeaf5f6cdd3f913ac85037730226f6066e1e7f96113fbb484"};
void score(struct Program * program) {

    int sc;
    program->runtime = 0;
    char output[256];

    for (int i = 0; i < 3; i++) {
        run(program, inputs[i], output);

        if (program->runtime == MAX_RUNTIME) {
            program->score = WORST;
            return;
        }

        int diff = strlen(outputs[i]) - strlen(output);
        sc = abs(diff) * 255;
        int compare = (diff > 0) ? strlen(output) : strlen(outputs[i]);
        for (int j = 0; j < compare; j++)
            sc += abs(outputs[i][j] - output[j]);
    }
    if (sc)
        program->score = sc * 50 + program->runtime + program->size * 2;
    else
        program->score = 0; // If no difference it's goolden
}

int main() {
    // srand(time(NULL));
    srand(50);
    printf("Size = %ld\n", sizeof(struct Program));
    struct Program * children = malloc(sizeof(struct Program) * NUM_CHILD);
    struct Program parent[NUM_WIN] = {0};
    size_t randomness = DEFAULT_RANDOMNESS;

    // Set the default
    for(int ancestor = 0; ancestor < NUM_WIN; ancestor++)
        parent[ancestor] = ancestor_prog();

    int times = 0;

    for(int runs = 0; runs < EXECUTIONS; runs++) {
        // Evolve from winning pool
        for (int winner = 0; winner < NUM_WIN; winner++) {
            // Keep the winner around so you never regress (agamogenesis)
            children[winner * NUM_GRAND] = parent[winner];
            for (int grandchild = 1; grandchild < NUM_GRAND; grandchild++)
                children[winner * NUM_GRAND + grandchild] = evolve(parent[winner], randomness);
        }
        for (int i = 0; i < NUM_CHILD; i++)
            score(&children[i]);

        qsort(children, NUM_CHILD, sizeof(struct Program), compare_ratings);

        if (children[0].score == parent[0].score)
            times++;
        else
            times = 0;

        printf("Winner of generation %d won with a score of %ld, size %ld, runtime %ld! (%d previously wins):\n", runs, children[0].score, children[0].size, children[0].runtime, times);
        write(1, children[0].code, children[0].size);
        puts("");

        randomness = DEFAULT_RANDOMNESS - times;
        if (children[0].score == 0) {
            puts("Solved");
            runs = EXECUTIONS;
        }
        else if (randomness == 50) {
            puts("I give up");
            runs = EXECUTIONS;
        }

        // Get winning pool, shoot for diversity
        parent[0] = children[0];
        int found = 1;
        for (int candidate = 1; candidate < NUM_CHILD; candidate++) {
            /* 
             * If we haven't found all winners, and this one is not identical to the previous winner
             * Can still have duplicates probably but not all duplicates, at least some genetic variety
             * If they're all the same hypothetically, then some left over dna from last generation would stay
             * Conveniently, the worse dna of the batch because the winners will stay winners
             * Frees everything we need, since parent and children share a pool
             */
            if (found < NUM_WIN && compare_code(children[candidate], parent[found - 1]))
                parent[found++] = children[candidate];
        }
        while (found < NUM_WIN)
            parent[found++] = parent[0];
    }

    free(children);
    return 0;
}

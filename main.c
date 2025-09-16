#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <main.h>

#define EXECUTIONS 10

#define NUM_WIN 100
#define NUM_CHILD NUM_WIN * NUM_WIN
#define NUM_GRAND NUM_CHILD / NUM_WIN

#define DEFAULT_RANDOMNESS 1050
struct Program def_prog = {0};

void set_default(struct Program new_def) {
    free(def_prog.code);
    def_prog.code = memcpy(malloc(new_def.size), new_def.code, new_def.size);
    def_prog.size = new_def.size;
    def_prog.score = new_def.score;
}

void free_default() {
    free(def_prog.code);
}

int compare_ratings(const void * first, const void * second) {
    // First minus second because we now want to sort in ascending order
    return ((struct Program *) first)->score - ((struct Program *) second)->score; 
}

bool compare_code(struct Program first, struct Program second) {
    if (first.size != second.size)
        return true;
    for (int i = 0; i < first.size; i++)
        if (((char *)first.code)[i] != ((char *)second.code)[i]) // Compare as strings rather than voids
            return true;
    return false;
}

unsigned int score(struct Program program) {
    unsigned char memory[65536] = {0};
    char * input = "Macy";

    for(int x = 0; x < strlen(input); x++)
        memory[x] = input[x];

    char * response = run(program, memory);
    char * word = "Bitter";

    if (response == NULL)
        return WORST;

    int diff = strlen(word) - strlen(response);
    int sc = abs(diff) * 255;
    int compare = (diff > 0) ? strlen(response) : strlen(word);
    for (int i = 0; i < compare; i++)
        sc += abs(word[i] - response[i]);

    sc *= 50;

    return sc;
}

struct Program get_default () {
    return (struct Program) {.code = memcpy(malloc(def_prog.size), def_prog.code, def_prog.size), .size = def_prog.size};
}

int main() {
    srand(time(NULL));
    struct Program children[NUM_CHILD] = {0};
    struct Program parent[NUM_WIN] = {0};
    size_t randomness = DEFAULT_RANDOMNESS;

    set_default(ancestor_prog());
    // Set the default
    for(int ancestor = 0; ancestor < NUM_WIN; ancestor++)
        parent[ancestor] = get_default();

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
            children[i].score = score(children[i]);

        qsort(children, NUM_CHILD, sizeof(struct Program), compare_ratings);

        if (children[0].score == def_prog.score)
            times++;
        else {
            times = 0;
            set_default(children[0]);
        }

        printf("Winner of generation %d won with a score of %d and size %d! (%d previously wins):\n", runs, children[0].score, children[0].size, times);
        char * compiled_code = debug(children[0]);
        printf(compiled_code);
        free(compiled_code);

        randomness = DEFAULT_RANDOMNESS - times;
        if (children[0].score == children[0].size) {
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
            else
                free(children[candidate].code);
        }
        while (found < NUM_WIN)
            parent[found++] = get_default();
    }

    for (int to_free = 0; to_free < NUM_WIN; to_free++)
        free(parent[to_free].code);
    free_default();
    return 0;
}

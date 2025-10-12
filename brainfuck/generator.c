#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <evolver.h>

char rand_dna() {
    char * chars = "+-<>[].0,";
    return chars[rand() % strlen(chars)];
}

struct Program evolve(struct Program prog, size_t randomness) {
    struct Program child;
    child.size = 0;

    int max_additions = MAX_SIZE - prog.size;
    int added = 0;

    for (int gene = 0; gene < prog.size; gene++) {
        int decision = rand() % randomness;
        switch (decision) {
            case 0: // 1 % chance you remove code
                added--;
                break;
            case 1: // 1 % chance you add code
                if (added < max_additions) {
                    added++;
                    gene--;
                    child.code[child.size++] = rand_dna();
                }
                break;
            case 2:
            case 3: // 3% chance you modify
            case 4:
                child.code[child.size++] = rand_dna();
                break;
            default: // 95 % chance you do nothing (slightly more because additions go here after)
                child.code[child.size++] = prog.code[gene];
                break;
        }
    }

    return child;
}

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <main.h>

char rand_dna() {
    char * chars = "+-<>[].";
    return chars[rand() % 7];
}

struct Program evolve(struct Program prog, size_t randomness) {
    char * parent = prog.code;

    int additions = rand() % 20;
    size_t new_len = prog.size + additions;

    char * child = malloc(new_len + 1); //In case it fills, the 0
    int added = 0;
    size_t index = 0;

    for (int gene = 0; gene < prog.size; gene++) {
        int decision = rand() % randomness;
        switch (decision) {
            case 0: // 1 % chance you remove code
                added--;
                break;
            case 1: // 1 % chance you add code
                if (added < additions) {
                    added++;
                    gene--;
                    child[index++] = rand_dna();
                }
                break;
            case 2:
            case 3: // 3% chance you modify
            case 4:
                child[index++] = rand_dna();
                break;
            default: // 95 % chance you do nothing (slightly more because additions go here after)
                child[index++] = parent[gene];
                break;
        }
    }

    child[index] = 0;
    return (struct Program){.code = child, .size = index};
}

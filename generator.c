#include <stdlib.h>
#include <string.h>

char rand_dna() {
    char * chars = "+-<>[]";
    return chars[rand() % 6];
}

char * evolve(char * parent) {
    size_t len = (parent == NULL) ? 0 : strlen(parent);
    
    char * child = malloc(len * 2); //Maximum new length, if every switch case is decided as an addition
    size_t index = 0;

    for (int gene = 0; gene < len; gene) {
        int decision = rand() % 50;
        switch (decision) {
            case 0: // 2 % chance you remove code
                break;
            case 1: // 2 % chance you add code
                child[index] = rand_dna();
                index++; // End so you don't add infinite code
            default: // 90 % chance you do nothing
                child[index] = parent[gene];
                index++;
                break;
            case 47:
            case 48: // 6% chance you modify
            case 49:
                child[index] = rand_dna();
                index++;
                break;
        }
    }

    free(parent);
    return child;
}
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char rand_dna() {
    char * chars = "+-<>[]";
    return chars[rand() % 6];
}

char * evolve(char * parent) {
    if (parent == NULL)
        parent = "+++++";
    int additions = rand() % 20;
    size_t new_len = strlen(parent) + additions;

    char * child = malloc(new_len + 1); //In case it fills, the 0
    int added = 0;
    size_t index = 0;

    for (int gene = 0; gene < strlen(parent); gene++) {
        int decision = rand() % 100;
        switch (decision) {
            case 0: // 1 % chance you remove code
                added--;
                break;
            case 1: // 1 % chance you add code
                if (added < additions) {
                    added++;
                    child[index++] = rand_dna();
                }
            default: // 95 % chance you do nothing (slightly more because additions go here after)
                child[index++] = parent[gene];
                break;
            case 47:
            case 48: // 3% chance you modify
            case 49:
                child[index++] = rand_dna();
                break;
        }
    }

    child[index] = 0;
    return child;
}
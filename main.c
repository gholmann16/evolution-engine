#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "game.h"

#define AMOUNT 100

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
    struct Program children[AMOUNT] = {0};

    for (int i = 0; i < AMOUNT; i++) {
        children[i].score = game(children[i].brainfuck);
    }

    qsort(children, AMOUNT, sizeof(struct Program), compare_ratings);

    // FILE * f = fopen("bf", "r");

    // fseek(f, 0L, SEEK_END);
    // size_t len = ftell(f);
    
    // fseek(f, 0L, SEEK_SET);	
    // char * contents = malloc(len);	
    
    // fread(contents, sizeof(char), len, f);
    // fclose(f);

    // puts(contents);
    // int score = game(starter);

    return 0;
}
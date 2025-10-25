#include <string.h>
#include <stdio.h>
#include <time.h>
#include "evolver.h"

char * jit(struct Program * code, char input[256], char output[256]);
void init_jit();
void free_jit();

int main() {
    char * code = def_code();
    struct Program prog = {
        .size = strlen(code),
        .runtime = 0,
        .score = 0,
    };
    strcpy(prog.code, code);
    char input[256] = "Hello wor\n";
    char output[256] = {0};
    clock_t begin = clock();
    run(&prog, input, output);
    clock_t end = clock();
    puts(output);
    printf("score time = %lf with %ld instructions\n", (double)(end - begin) / CLOCKS_PER_SEC, prog.runtime);

    prog.runtime = 0;
    char output2[256] = {0};
    init_jit();
    printf("versus\n");
    begin = clock();
    char * out = jit(&prog, input, output2);
    end = clock();
    puts(output);
    printf("score time = %lf\n", (double)(end - begin) / CLOCKS_PER_SEC);
    free_jit();
}

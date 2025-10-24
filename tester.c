#include <string.h>
#include <stdio.h>
#include <time.h>
#include "evolver.h"

char * jit(char * code, char input[256], char output[256]);

int main() {
    char * code = def_code();
    struct Program prog = {
        .size = strlen(code),
    };
    strcpy(prog.code, code);
    char input[256] = "Hello wor\n";
    char output[256];
    clock_t begin = clock();
    run(&prog, input, output);
    clock_t end = clock();
    puts(output);
    printf("score time = %lf\n", (double)(end - begin) / CLOCKS_PER_SEC);

    printf("versus\n");
    begin - clock();
    char * out = jit(code, input, output);
    end = clock();
    if(out)
        puts("success");
    printf("score time = %lf\n", (double)(end - begin) / CLOCKS_PER_SEC);

}

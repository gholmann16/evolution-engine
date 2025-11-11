#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "evolver.h"
#include "tests/test.hpp"

Test * tester = create_crc8();

int main() {
    init_env();
    struct Program prog = ancestor_prog();
    alignas(256) char input[256] = {"hello brainfucke"};
    char expect[256];
    tester->answer(input, expect);
    alignas(256) char output[256] = {0};
    run(&prog, input, output, tester->max_runtime());
    if (prog.runtime == tester->max_runtime())
        puts("exit");

    int diff = strlen(expect) - strlen(output);
    int max = (diff < 0) ? strlen(expect) : strlen(output);
    prog.score += 255 * abs(diff);
    for (int ch = 0; ch < max; ch++) {
        prog.score += abs(expect[ch] - output[ch]);
        printf("at %d %02x and %02x are different\n", ch, expect[ch], output[ch]);
    }
    printf("initial score of %ld, %ld, %ld, output %s vs %s\n", prog.score, prog.size, prog.runtime, output, expect);

    free_env();
    // char output[256] = {0};
    // clock_t begin = clock();
    // run(&prog, input, output);
    // clock_t end = clock();
    // puts(output);
    // printf("score time = %lf with %ld instructions\n", (double)(end - begin) / CLOCKS_PER_SEC, prog.runtime);

    // prog.runtime = 0;
    // alignas(256) char output2[256] = {0};
    // init_env();
    // printf("versus\n");
    // begin = clock();
    // run(&prog, input, output2);
    // end = clock();
    // puts(output2);
    // printf("score time = %lf with %ld instructions\n", (double)(end - begin) / CLOCKS_PER_SEC, prog.runtime);
    // free_env();
}

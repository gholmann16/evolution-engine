#include <string.h>
#include <stdio.h>
#include "evolver.h"

int main() {
    char * code = ">>+<--[[<++>->-->+++>+<<<]-->++++]<<.<<-.<<..+++.>.<<-.>.+++.------.>>-.<+.>>.";
    struct Program prog = {
        .size = strlen(code),
    };
    strcpy(prog.code, code);
    char input[256];
    char output[256];
    run(&prog, input, output);
    puts(output);
}

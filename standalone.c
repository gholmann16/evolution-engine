#include <stdio.h>
#include <stdlib.h>
#include "sucrisc.h"
#include "compiler.h"
#include "vector.h"

int main() {
    struct Program test = {
        .code = malloc(3 * sizeof(short)),
        .size = 3,
    };
    test.code[0] = 0b0101000001000010;
    test.code[1] = 0b0101000100000001;
    test.code[2] = 0b0110000100000000;
    char * code = compile(test);
    printf(code);
    free(code);
    free(test.code);
    return 0;
}
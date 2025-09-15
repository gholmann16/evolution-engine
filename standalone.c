#include <stdio.h>
#include <stdlib.h>
#include "sucrisc.h"
#include "compiler.h"
#include "vector.h"

int main() {
    struct Program test = {
        .code = malloc(sizeof(short)),
        .size = 1,
    };
    test.code[0] = 0b0111000001000010;
    char * code = compile(test);
    printf(code);
    free(code);
    free(test.code);
    return 0;
}
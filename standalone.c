#include <stdio.h>
#include <stdlib.h>
#include "sucrisc.h"
#include "compiler.h"
#include "vector.h"

int main() {
    struct Program test = {
        .code = malloc(1 * sizeof(union Operation)),
        .size = 1,
    };
    test.code[0] = (union Operation) {
        .opcode = MOVM,
        .to_reg = 0, // Starts holding value of 0
        .or_num = true,
        .fr_mem = true, // Ignored anyway
        .number = 'B',
    };
    char * code = compile(test);
    printf(code);
    free(code);
    free(test.code);
    return 0;
}
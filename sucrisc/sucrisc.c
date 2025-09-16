#include <stdlib.h>
#include <stdio.h>
#include "sucrisc.h"
#include "../main.h"
#include "vector.h"

#define MAX_RUNTIME 100000

union Operation def_op = {
    .opcode = MOVM,
    .to_reg = 0, // Starts holding value of 0
    .or_num = true,
    .fr_mem = true, // Ignored anyway
    .number = 'B',
};

struct Program ancestor_prog() {
    return (struct Program){.code = &def_op, .size = sizeof(union Operation), .score = WORST};
}

struct Program evolve(struct Program prog, size_t randomness) {
    union Operation * parent = prog.code;
    unsigned int index = 0;
    unsigned int max_adds = 100;
    int adds = 0;
    union Operation * child = malloc(prog.size + 100*sizeof(union Operation));

    for (unsigned int gene = 0; gene < (prog.size / sizeof(union Operation)); gene++) {
        if (rand() % randomness < 2) { // 2 / randomness chance you or subtract a gene
            if(rand() % 2 && adds < max_adds) { // 1/2 chance you add a gene, generate random number and rewind gene so it still adds the next one
                child[index++].raw = rand();
                gene--;
                adds++;
            }
            else
                adds--;
            continue; // To subtract a gene simply just go to the next cycle
        }
        for (unsigned int bit = 0; bit < sizeof(union Operation); bit++) {
            if (!(rand() % randomness)) // 1 / randomness chance
                parent[gene].raw ^= (1 << bit);
        }
        // After potential changing code, randomize
        child[index++] = parent[gene];
    }
    return (struct Program){.code = child, .size = prog.size + adds*sizeof(union Operation)};
}

unsigned short value(union Operation op, unsigned char memory[65536], unsigned short reg[16], unsigned int size, unsigned int index) {
    if (op.or_num) // Get direct number
        return op.number; // Essentially a char
    return (op.fr_mem) ? memory[reg[op.fr_reg]] : reg[op.fr_reg]; //Either char or short
}

char * run(struct Program prog, unsigned char memory[65536]) {
    union Operation * code = prog.code;
    unsigned short reg[16] = {0};

    static struct Vector points = {0};
    if (points.capacity == 0)
        points = init_vec();
    clear_vec(&points);

    unsigned int runtime = 0;
    for (unsigned int index = 0; index < (prog.size / sizeof(union Operation)); index++) {
        if (++runtime == MAX_RUNTIME)
            return NULL;

        unsigned short val = value(code[index], memory, reg, prog.size, index);

        switch(code[index].opcode) { //gets first 2 bits
            case ADD: // Add
                reg[code[index].to_reg] += val;
                break;
            case ADDM:
                memory[reg[code[index].to_reg]] += val;
                break;
            case MOV: // Mov
                reg[code[index].to_reg] = val;
                break;
            case MOVM:
                memory[reg[code[index].to_reg]] = val;
                break;
            case PT: // Point
                push(&points, index);
                break;
            case NOP:
                break;
            case JE : // Jump to point if the 2 conditions are equal
                unsigned int point = pop(&points);
                if (point == -1)
                    return NULL;
                if (reg[code[index].to_reg] == val)
                    index = point - 1; //Queue again
                break;
            case JEM:
                unsigned int pointm = pop(&points);
                if (pointm == -1)
                    return NULL;
                if(memory[reg[code[index].to_reg]] == val)
                    index = pointm - 1; //Queue again
                break;
        }
    }

    return (char *)&memory[reg[0]];
}

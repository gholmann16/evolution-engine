#include <stdlib.h>
#include <stdio.h>
#include "sucrisc.h"
#include "vector.h"

extern unsigned char memory[65536];
extern int randomness;
unsigned short reg[16];

struct Program evolve(union Operation * parent, unsigned int size) {
    unsigned int index = 0;
    unsigned int max_adds = size + 100;
    int adds = 0;
    union Operation * child = malloc(sizeof(union Operation) * (size + max_adds));

    for (unsigned int gene = 0; gene < size; gene++) {
        if (!(rand() % randomness)) { // 1 / randomness chance you or subtract a gene
            if(rand() % 2 && size+adds < size) { // 1/2 chance you add a gene, generate random number and rewind gene so it still adds the next one
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
    return (struct Program){.code = child, .size = size+adds};
}

void clear() {
    for (int i = 0; i < 16; i++)
        reg[i] = 0;
    for (int j = 0; j < sizeof(memory); j++)
        memory[j] = 0;
}

unsigned short value(union Operation op) {
    if (op.or_num) // Get direct number
        return op.number; // Essentially a char

    return (op.fr_mem) ? memory[reg[op.fr_reg]] : reg[op.fr_reg]; //Either char or short
}

char * run(union Operation * code, unsigned int size, char * input, unsigned int isize) {

    clear();
    for(int x = 0; x < isize; x++)
        memory[x] = input[x];

    static struct Vector points = (struct Vector){0};
    if (points.capacity == 0)
        points = init_vec();

    for (unsigned int index = 0; index < size; index++) {
        unsigned short val = value(code[index]);

        switch(code[index].opcode) { //gets first 2 bits
            case ADD: // Add
                reg[code[index].to_reg] += val;
                break;
            case ADDM:
                memory[reg[code[index].to_reg]] += val;
            case MOV: // Mov
                reg[code[index].to_reg] += val;
                break;
            case MOVM:
                memory[reg[code[index].to_reg]] += val;
            case PT: // Point
                push(&points, index);
                break;
            case NOP:
                break;
            case JE : // Jump to point if the 2 conditions are equal
                unsigned int point = pop(&points);
                if (reg[code[index].to_reg] == val)
                    index = point - 1; //Queue again
            case JEM:
                unsigned int pointm = pop(&points);
                if(memory[reg[code[index].to_reg]] == val)
                    index = pointm - 1; //Queue again
                break;
        }
    }

    clear_vec(&points);
    return (char *)&memory[reg[0]];
}

#include <stdlib.h>
#include <stdio.h>
#include "sucrisc.h"
#include "vector.h"

extern char memory[65536];
extern int randomness;
unsigned short reg[16];

unsigned short value(short opcode) {
    if (opcode & OR_NUM_MASK) // Get direct number
        return opcode & DIRNUM_MASK; // Essentially a char

    int regnum = opcode & FROMREGMASK;
    return (opcode & FROMMEMMASK) ? memory[reg[regnum]] : reg[regnum]; //Either char or short
}

struct Program evolve(short * parent, unsigned int size) {
    unsigned int index = 0;
    short * child = malloc(size*2);
    int adds = 0;

    for (unsigned int gene = 0; gene < size; gene++) {
        if (!(rand() % randomness)) { // 1 / randomness chance you or subtract a gene
            if(rand() % 2 && adds+2 <= size) { // 1/2 chance you add a gene, generate random number and rewind gene so it still adds the next one
                child[index++] = rand();
                gene--;
                adds++;
            }
            else
                adds--;
            continue; // To subtract a gene simply just go to the next cycle
        }
        for (unsigned int bit = 0; bit < sizeof(short); bit++) {
            if (!(rand() % randomness)) // 1 / randomness chance
                parent[gene] ^= (1 << bit);
        }
        // After potential changing code, randomize
        child[index++] = parent[gene];
    }

    struct Program ret = {
        .code = child, 
        .size = size+adds
    };
    return ret;
}

void clear() {
    for (int i = 0; i < 16; i++)
        reg[i] = 0;
    for (int j = 0; j < sizeof(memory); j++)
        memory[j] = 0;
}

char * run(short * code, unsigned int size, char * input, unsigned int isize) {

    clear();
    for(int x = 0; x < isize; x++)
        memory[reg[0] + x] = input[x];

    static struct Vector points = (struct Vector){0};
    if (points.capacity = 0)
        points = init_vec();

    for (unsigned int index = 0; index < size; index++) {
        unsigned short val = value(code[index]);
        int regnum = (code[index] & TO_REG_MASK) >> TO_REG_SHIFT;

        switch(code[index] & OPCODE_MASK) { //gets first 2 bits
            case ADD: // Add
                if (code[index] & TO_MEM_MASK)
                    memory[reg[regnum]] += val;
                else
                    reg[regnum] += val;
                break;
            case MOV: // Mov
                if (code[index] & TO_MEM_MASK)
                    memory[reg[regnum]] += val;
                else
                    reg[regnum] += val;
                break;
            case PT : // Point
                push(&points, index);
                break;
            case JE : // Jump to point if the 2 conditions are equal
                unsigned int point = pop(&points);
                if (((code[index] & TO_MEM_MASK) ? memory[reg[regnum]] : reg[regnum]) == val) //Knows that val can't be a short (less useful)
                    index = point - 1; //Queue again
        }
    }

    clear_vec(&points);
    return &memory[reg[1]];
}

#include <stdlib.h>
#include "sucrisc.h"
#include "vector.h"

extern char memory[65536];
extern int randomness;
unsigned short registers[16];

unsigned short value(short opcode) {
    if (opcode & OR_NUM_MASK) // Get direct number
        return opcode & DIRNUM_MASK;

    int regnum = opcode & FROMREGMASK;
    unsigned short tmp;
    if (opcode & FROMMEMMASK) // Pull from memory
        tmp = memory[registers[regnum]];
    else // Raw regnum
        tmp = registers[regnum];

    return tmp + opcode & OFFSET_MASK - 4; // Offset it, subtract 4 always so it can be negative.
}

void * operate_on(char opcode) {
    int regnum = opcode & TO_REG_MASK;

    if (opcode & TO_MEM_MASK)
        return (unsigned short *)&memory[registers[regnum]];
    else
        return &registers[regnum];
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
        registers[i] = 0;
    for (int j = 0; j < sizeof(memory); j++)
        memory[j] = 0;
}

char * run(short * code, unsigned int size, char * input, unsigned int isize) {

    clear();
    for(int x = 0; x < isize; x++)
        memory[registers[0] + x] = input[x];

    struct Vector points = init();
    for (unsigned int index = 0; index < size; index++) {
        unsigned short val = value(code[index]);
        unsigned short * ptr = operate_on(code[index]);
        switch(code[index] & OPCODE_MASK) { //gets first 2 bits
            case 0b00: // Add
                *ptr = *ptr + val;
                break;
            case 0b01: // Mov
                *ptr = val;
                break;
            case 0b10: // Point
                push(points, index);
                break;
            case 0b11: // Jump to point if the 2 conditions are equal
                unsigned int point = pop(points);
                if (*ptr == val)
                    index = point - 1; //Queue again
        }
    }

    return &memory[registers[1]];
}

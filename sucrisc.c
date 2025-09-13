#include <stdlib.h>
#include "sucrisc.h"

#define OPCODE_MASK 0b1100000000000000 // First 2 bits
#define TO_MEM_MASK 0b0010000000000000 // Push to register memory
#define OR_NUM_MASK 0b0001000000000000 // Does it come from a number (origin number mask)
#define TO_REG_MASK 0b0000111100000000 // Operating register
#define FROMMEMMASK 0b0000000010000000 // Pull from register memory?
#define OFFSET_MASK 0b0000000001110000 // What numerical offset
#define FROMREGMASK 0b0000000000001111 // What is the operand register
#define DIRNUM_MASK 0b0000000011111111 // Direct number

extern char memory[65537]; // 1 extra memory just in case
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

    for (unsigned int index = 0; index < size; index++) {
        unsigned short val = value(code[index]);
        unsigned short * ptr = operate_on(code[index]);
        switch(code[index] & OPCODE_MASK) { //gets first 2 bits
            case 0b00: // Add
                *ptr += *ptr + val;
            case 0b01: // Sub
                *ptr = *ptr + val;
            case 0b10: // Mov
                *ptr = *ptr + val;
            case 0b11:
                if (*ptr == 0)
                    index += val;
        }
    }

    return &memory[registers[1]];
}

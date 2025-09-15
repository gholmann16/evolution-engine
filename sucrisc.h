#define OPCODE_MASK 0b1100000000000000 // First 2 bits
#define TO_MEM_MASK 0b0010000000000000 // Push to register memory
#define OR_NUM_MASK 0b0001000000000000 // Does it come from a number (origin number mask)
#define TO_REG_MASK 0b0000111100000000 // Operating register
#define FROMMEMMASK 0b0000000010000000 // Pull from register memory?
#define USELESSMASK 0b0000000001110000 // What numerical offset
#define FROMREGMASK 0b0000000000001111 // What is the operand register
#define DIRNUM_MASK 0b0000000011111111 // Direct number
#define MEMREG_MASK 0b0010000010000000 // Test what combination of register and memory op it is

#define MEM_TO_MEM 0b0010000010000000 // Operating on memory from memory (8 bit each)
#define MEM_TO_REG 0b0010000000000000 // Operating on register from memory (8 bit needs to go to 16)
#define REG_TO_MEM 0b0000000010000000 // Operating on memory from register (16 bit needs to go to 8)
#define REG_TO_REG 0b0000000000000000 // Operating on register from register (16 bit each)

#define TO_REG_SHIFT 8

#define ADD 0b0000000000000000
#define MOV 0b0100000000000000
#define PT  0b1000000000000000
#define JE  0b1100000000000000

struct Program {
    unsigned short * code;
    unsigned int size;
    unsigned int score;
};

char * run(short * code, unsigned int size, char * input, unsigned int isize);
struct Program evolve(short * code, unsigned int size);
int validate(char * code, unsigned int size);

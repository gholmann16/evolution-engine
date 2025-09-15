union Operation {
    unsigned short raw;
    struct {
        unsigned short opcode : 3;
        unsigned short to_reg : 3; // Register on which memory to operate on
        unsigned short or_num : 1; // Does it come from a number (origin number mask)
        unsigned short fr_mem : 1; // Pull from register memory (if not from number)
        union { // Could be a numerical value or a register + useless
            unsigned short number : 8; // Operand, data being used 
            struct { // payload bit field
                unsigned char useles : 5;
                unsigned char fr_reg : 3; // Register data is coming from
            };
        };
    };
};


#define ADD  0b000
#define MOV  0b001
#define PT   0b010
#define JE   0b011
#define ADDM 0b100
#define MOVM 0b101
#define NOP  0b110
#define JEM  0b111

struct Program {
    union Operation * code;
    unsigned int size;
    unsigned int score;
};

char * run(union Operation * code, unsigned int size, char * input, unsigned int isize);
struct Program evolve(union Operation * parent, unsigned int size);

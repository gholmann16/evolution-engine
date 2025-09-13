struct Program {
    unsigned short * code;
    unsigned int size;
    unsigned int score;
};

char * run(short * code, unsigned int size, char * input, unsigned int isize);
struct Program evolve(short * code, unsigned int size);

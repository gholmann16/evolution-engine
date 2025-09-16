#define WORST 50000

struct Program {
    void * code;
    unsigned int size;
    unsigned int score;
};

char * run(struct Program prog, unsigned char memory[65536]); // Should be non freeable memory, too many allocs otherwise
char * debug(struct Program prog); // Should be an allocated string

struct Program ancestor_prog();
struct Program evolve(struct Program parent, size_t randomness); 
char * compile(struct Program program); // Allocated string ready to be passed to assembler

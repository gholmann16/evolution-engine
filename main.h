#define WORST       5000000
#define MAX_RUNTIME 500000
#define MAX_SIZE    5000

struct Program {
    char code[1000]; // No 0 at the end, don't use strlen
    size_t size;
    size_t runtime;
    size_t score;
};

// Input should be read only
void run(struct Program * prog, char input[256], char output[256]); // Should be non freeable memory, too many allocs otherwise
char * debug(struct Program prog); // Should be an allocated string

struct Program ancestor_prog();
struct Program evolve(struct Program parent, size_t randomness); 
char * compile(struct Program program); // Allocated string ready to be passed to assembler

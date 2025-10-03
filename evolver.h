#include <stddef.h>
#define WORST       5000000
#define MAX_RUNTIME 500000
#define MAX_SIZE    5000

struct Program {
    char code[1000]; // No 0 at the end, don't use strlen
    size_t size;
    size_t runtime;
    size_t score;
};

struct State {
    int seed;
    int runs;
    int total_winners;
    int def_rand;
    int repetitions;
    struct Program * parent; // struct Program * total_winners
    struct Program * children; // struct Program * total_winners^2
};

// Running
struct State load(char * file);
struct State def_state();
int evolver(struct State);

// Executing
void run(struct Program * prog, char input[256], char output[256]); // input read only, output should be non freeable memory, too many allocs otherwise

// Evolving
struct Program ancestor_prog();
struct Program evolve(struct Program parent, size_t randomness); 

// Utility
char * debug(struct Program prog); // Should be an allocated string
char * compile(struct Program program); // Allocated string ready to be passed to assembler

#include <stddef.h>
#define MAX_SIZE 65533

#ifdef __cplusplus
extern "C" {
#endif

struct Program {
    char code[MAX_SIZE]; // No 0 at the end, don't use strlen
    size_t size;
    size_t runtime;
    size_t score;
};

struct State {
    long int seed;
    int runs;
    int total_winners;
    int def_rand;
    int repetitions;
    struct Program * parent; // struct Program * total_winners
    struct Program * children; // struct Program * total_winners^2
};

// Init
struct State def_state();

// Executing
void init_env();
bool generation(struct State state);
void run(struct Program * prog, char input[256], char output[256], size_t max); // input read only, output should be non freeable memory, too many allocs otherwise
void free_env();

// Evolving
struct Program ancestor_prog();
struct Program evolve(struct Program parent, size_t randomness, const char * allowed_chars); 

// Utility
char * debug(struct Program prog); // Should be an allocated string
char * compile(struct Program program); // Allocated string ready to be passed to assembler

#ifdef __cplusplus
}
#endif

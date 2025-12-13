#include <state.hpp>
#include <evolver.hpp>
#include <time.h>
#define NUM_WIN 100
#define DEFAULT_RANDOMNESS 1050

struct State def_state() {
    return (struct State) {
        .seed = time(NULL),
        .total_winners = NUM_WIN,
        .def_rand = DEFAULT_RANDOMNESS,
        .children = new Program[NUM_WIN * NUM_WIN],
    };
}

// void save(struct State state) {
//     FILE * out = fopen("save.bin", "wb");
//     fwrite(&state, sizeof(struct State), 1, out);
//     fwrite(state.children, sizeof(struct Program), state.total_winners, out);
//     fclose(out);
// }

struct State load(char * file) {
    return def_state();
}

// struct State load(char * file) {
//     FILE * in = fopen(file, "rb");
//     struct State revived;
//     fread(&revived, sizeof(struct State), 1, in);
//     revived.parent = (struct Program *) malloc(sizeof(struct Program) * revived.total_winners);
//     revived.children = (struct Program *) malloc(sizeof(struct Program) * revived.total_winners * revived.total_winners);
//     fread(revived.parent, sizeof(struct Program), revived.total_winners, in);
//     fclose(in);
//     return revived;
// }

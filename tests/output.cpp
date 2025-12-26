#include <string.h>
#include "test.hpp"

#define MAX_RUNTIME 100000

class Output : public Test {
    public:
        void score(struct Program * prog, Engine * engine, const State * state) override {
            alignas(256) char empty_input[256] = {0};
            alignas(256) char output[256] = {0};
            if (engine->run(prog->code, empty_input, output, MAX_RUNTIME) == MAX_RUNTIME) {
                prog->score = MAX_RUNTIME * 50;
                return;
            }

            prog->score = 0;
            const char * expected = "Hello World";

            int diff = strlen(expected) - strlen(output);
            int max = (diff < 0) ? strlen(expected) : strlen(output);
            prog->score += 255 * abs(diff);
            for (int ch = 0; ch < max; ch++) {
                prog->score += abs(expected[ch] - output[ch]);
            }

            if (prog->score) {
                prog->score = prog->score * 50 + engine->size(prog->code);
            }
        }
};

Test * create_output() {
    return new Output();
}

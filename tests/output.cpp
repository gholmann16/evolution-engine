#include <string.h>
#include "test.hpp"

#define MAX_RUNTIME 100000

class Output : public Test {
    public:
        unsigned long long score(const void * code, Engine * engine, const State * state) override {
            alignas(256) char empty_input[256] = {0};
            alignas(256) char output[256] = {0};
            if (engine->run(code, empty_input, output, MAX_RUNTIME) == MAX_RUNTIME)
                return MAX_RUNTIME * 50;

            unsigned long long score = 0;
            const char * expected = "Hello World";

            int diff = strlen(expected) - strlen(output);
            int max = (diff < 0) ? strlen(expected) : strlen(output);
            score += 255 * abs(diff);
            for (int ch = 0; ch < max; ch++) {
                score += abs(expected[ch] - output[ch]);
            }

            return score ? score * 50 + engine->size(code) : 0;
        }
};

Test * create_output() {
    return new Output();
}

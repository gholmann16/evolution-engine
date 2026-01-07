#include <string.h>

#define MAX_RUNTIME 100000

class Output : public Test {
    public:
        size_t score(const void * code) const override {
            alignas(256) char empty_input[256] = {0};
            alignas(256) char output[256] = {0};
            if (State::engine->run(code, empty_input, output, MAX_RUNTIME) == MAX_RUNTIME)
                return MAX_RUNTIME * 50;

            size_t total_score = 0;
            const char * expected = "Hello World";

            int diff = strlen(expected) - strlen(output);
            int max = (diff < 0) ? strlen(expected) : strlen(output);
            total_score += 255 * abs(diff);
            for (int ch = 0; ch < max; ch++) {
                total_score += abs(expected[ch] - output[ch]);
            }

            return total_score ? total_score * 50 + State::engine->size(code) : 0;
        }
};

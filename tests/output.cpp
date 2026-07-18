#include <string.h>

#define MAX_RUNTIME 100000

class Output : public Test {
    private:
        static constexpr const char * REFERENCE_INPUT = "Hello papa";
    public:
        size_t score(const void * code) const override {
            alignas(256) char input[256] = {0};
            strncpy(input, REFERENCE_INPUT, sizeof(input));
            alignas(256) char output[256] = {0};
            State::engine->load(code);

            if (State::engine->run(input, output, MAX_RUNTIME) == MAX_RUNTIME)
                return MAX_RUNTIME * 50;

            size_t total_score = 0;
            const char * expected = "Hello World";

            // output is a raw 256-byte buffer, not guaranteed null-terminated
            // (an evolved program can fill every byte via '.'); strnlen bounds
            // the scan instead of running off the end like strlen would.
            int diff = strlen(expected) - strnlen(output, sizeof(output));
            int max = (diff < 0) ? strlen(expected) : strnlen(output, sizeof(output));
            total_score += 255 * abs(diff);
            for (int ch = 0; ch < max; ch++) {
                total_score += abs(expected[ch] - output[ch]);
            }

            return total_score ? total_score * 50 + State::engine->size(code) : 0;
        }

        void reference_input(char input[256]) const override {
            strncpy(input, REFERENCE_INPUT, 256);
        }

        std::string display(const void * code) const override {
            alignas(256) char input[256] = {0};
            strncpy(input, REFERENCE_INPUT, sizeof(input));
            alignas(256) char output[256] = {0};
            State::engine->load(code);
            State::engine->run(input, output, MAX_RUNTIME);
            return std::string(output, strnlen(output, sizeof(output)));
        }
};

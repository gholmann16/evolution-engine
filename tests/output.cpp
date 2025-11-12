#include <string.h>
#include "test.hpp"

#define MAX_RUNTIME 100000

class Output : public Test {
    public:
        void prepare_answer() override {
            ;
        }
        void score(struct Program * prog) override {
            alignas(256) char empty_input[256] = {0};
            alignas(256) char output[256] = {0};
            prog->score = 0;
            run(prog, empty_input, output, MAX_RUNTIME);
            if (prog->runtime == MAX_RUNTIME) {
                prog->score = MAX_RUNTIME * 50;
                return;
            }

            const char * expected = "Hello World";
            int diff = strlen(expected) - strlen(output);
            int max = (diff < 0) ? strlen(expected) : strlen(output);
            prog->score += 255 * abs(diff);
            for (int ch = 0; ch < max; ch++) {
                prog->score += abs(expected[ch] - output[ch]);
            }

            if (prog->score) {
                prog->score = prog->score * 50 + prog->size;
            }
            else { // solution found
                prog->score = 0;
            }
        }

        void display(struct Program * prog) override {
            char empty_input[256] = {0};
            char output[256] = {0};
            run(prog, empty_input, output, MAX_RUNTIME);
            puts(output);
        }

        const char * allowed_chars() override {
            return "+-<>[].";
        }
};

Test * create_output() {
    return new Output();
}

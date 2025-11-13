#include <string.h>
#include <evolver.hpp>

#define MAX_RUNTIME 100000

class Output : public Test {
    public:
        void prepare_answer() override {
            ;
        }
        void score(struct Program * prog, Engine * engine) override {
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
                prog->score = prog->score * 50 + prog->code.size();
            }
        }

        void display(struct Program * prog, Engine * engine) override {
            char empty_input[256] = {0};
            char output[256] = {0};
            engine->run(prog->code, empty_input, output, MAX_RUNTIME);
            puts(output);
        }

        const char * allowed_chars() override {
            return "+-<>[].";
        }
};

Test * create_output() {
    return new Output();
}

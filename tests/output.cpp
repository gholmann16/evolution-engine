#include <string.h>

#define OUTPUT_TRIALS 10

class Output : public Test {
    private:
        // Same per-generation caching pattern as Add/Crc8: regenerate once
        // when a new generation starts scoring, not once per genome.
        alignas(256) inline static char noise_inputs[OUTPUT_TRIALS][256];
        inline static size_t current_generation = -1;

        static void prepare_inputs() {
            current_generation = State::runs;
            for (int t = 0; t < OUTPUT_TRIALS; t++)
                for (int i = 0; i < 256; i++)
                    noise_inputs[t][i] = rand() % 256;
        }

        // 0 = exact "Hello World" match; otherwise how far off, same
        // weighting score() used to apply to its single trial.
        static size_t trial_error(const char output[256]) {
            const char * expected = "Hello World";

            // output is a raw 256-byte buffer, not guaranteed null-terminated
            // (an evolved program can fill every byte via '.'); strnlen bounds
            // the scan instead of running off the end like strlen would.
            int diff = (int)strlen(expected) - (int)strnlen(output, 256);
            int max = (diff < 0) ? (int)strlen(expected) : (int)strnlen(output, 256);
            size_t error = 255 * (size_t)abs(diff);
            for (int ch = 0; ch < max; ch++)
                error += (size_t)abs(expected[ch] - output[ch]);
            return error;
        }

    public:
        // Input is random noise, regenerated every generation, in
        // OUTPUT_TRIALS different trials -- a program that hardcodes "Hello
        // World" off whatever garbage it happens to read (or just never
        // reads input at all) is exactly what this test is meant to reward,
        // since real input is never trustworthy.
        size_t score(const void * code) const override {
            if (current_generation != State::runs)
                prepare_inputs();

            State::engine->load(code);

            size_t total_error = 0;
            for (int t = 0; t < OUTPUT_TRIALS; t++) {
                alignas(256) char input[256];
                memcpy(input, noise_inputs[t], sizeof(input));
                alignas(256) char output[256] = {0};

                if (State::engine->run(input, output, State::max_runtime) == State::max_runtime)
                    return State::max_runtime * 50;

                total_error += trial_error(output);
            }

            return total_error ? total_error * 50 + State::engine->size(code) : 0;
        }

        // How many of this generation's noise inputs the genome turns into
        // an exact "Hello World" -- a count is far more meaningful than
        // showing one trial's raw text once there's more than one trial.
        std::string display(const void * code) const override {
            if (current_generation != State::runs)
                prepare_inputs();

            State::engine->load(code);
            int correct = 0;
            for (int t = 0; t < OUTPUT_TRIALS; t++) {
                alignas(256) char input[256];
                memcpy(input, noise_inputs[t], sizeof(input));
                alignas(256) char output[256] = {0};
                State::engine->run(input, output, State::max_runtime);
                if (trial_error(output) == 0)
                    correct++;
            }
            return std::to_string(correct) + "/" + std::to_string(OUTPUT_TRIALS) + " correct";
        }
};

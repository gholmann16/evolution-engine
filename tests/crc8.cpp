#include <string.h>

// Needed for little endian
#define SWAP(x) ((x>>8) & 0xff) | ((x & 0xff)<<8)
#define NUM_TRIES 3
size_t LOOP_MAX = 1000000000;

class Crc8 : public Test {
    private:
        // DVB-S2
        // Polynomial x^8 + x^7 + x^6 + x^4 + x^2 + x^0 shifted
        static constexpr unsigned short polynomials[8] = {
            SWAP(0x01D5 << 0), SWAP(0x01D5 << 1), SWAP(0x01D5 << 2), SWAP(0x01D5 << 3),
            SWAP(0x01D5 << 4), SWAP(0x01D5 << 5), SWAP(0x01D5 << 6), SWAP(0x01D5 << 7),
        };

        static char crc8(char input[256]) {
            size_t size = strlen(input);
            for (size_t byte = 0; byte < size; byte++) {
                // Obtain a short pointer for word work
                short * pointer = (short *)(input + byte);
                for (int bit = 7; bit >= 0; bit--) {
                    // If leading bit is 1
                    if (input[byte] >> bit)
                        pointer[0] ^= polynomials[bit];
                }
            }
            // Should already be allocated as originally null byte
            return input[size];
        }

        static void fill_string(char input[256]) {
            for (int i = 0; i < 255; i++) {
                input[i] = rand() % 256;
            }
            input[255] = 0;
        }

        inline static char ** inputs;
        inline static char ** expect;
        inline static size_t current_generation;

        static void prepare_answer() {
            current_generation = State::runs;
            char tmp[256];
            for (int i = 0; i < NUM_TRIES; i++) {
                free(inputs[i]);
                free(expect[i]);
                inputs[i] = (char *)malloc(256);
                fill_string(inputs[i]);
                expect[i] = (char *)malloc(2);

                memcpy(tmp, inputs[i], 256);
                expect[i][0] = crc8(tmp);
                expect[i][1] = 0;
            }
        }

    public:
        Crc8() {
            inputs = (char **)calloc(NUM_TRIES, sizeof(char *));
            expect = (char **)calloc(NUM_TRIES, sizeof(char *));
            current_generation = -1;
        }

        size_t score(const void * code) const override {
            if (current_generation != State::runs)
                prepare_answer();

            State::engine->load(code);

            int times = 0;
            size_t runtime = 0;
            while (times < NUM_TRIES) {
                alignas(256) char output[256] = {0};
                alignas(256) char input[256];
                memcpy(input, inputs[times], 256);

                runtime += State::engine->run(input, output, LOOP_MAX - runtime);
                if (runtime == LOOP_MAX || expect[times][0] != output[0])
                    return LOOP_MAX * 50;

                times++;
            }

            return runtime + State::engine->size(code) * 5;
        }

        ~Crc8() {
            free(inputs);
            free(expect);
        }
};

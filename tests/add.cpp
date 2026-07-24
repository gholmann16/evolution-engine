struct alignas(256) AlignedBuffer {
    char data[256];
};

class Add : public Test {
    private:
        inline static AlignedBuffer inputs[10];
        inline static unsigned char sum[10];
        inline static size_t current_generation = -1;

        static void prepare_answer() {
            current_generation = State::runs;
            for (int i = 0; i < 10; i++) {
                inputs[i].data[0] = rand() % 256;
                inputs[i].data[1] = rand() % 256;
                sum[i] = (unsigned char)(inputs[i].data[0] + (unsigned char)inputs[i].data[1]);
            }
        }

    public:
        size_t score(const void * code) const override {
            if (current_generation != State::runs)
            prepare_answer();

            State::engine->load(code);

            size_t runtime = 0;
            size_t error = 0;

            for (int i = 0; i < 10; i++) {
                alignas(256) char output[256] = {0};

                runtime += State::engine->run(inputs[i].data, output, State::max_runtime - runtime);
                if (runtime == State::max_runtime)
                    return State::max_runtime * 50;

                error += abs(sum[i] - (unsigned char)output[0]);
            }

            return error ? runtime + State::engine->size(code) * 5 + error * 10000 : 0;
        }

        std::string display(const void * code) const override {
            if (current_generation != State::runs)
                prepare_answer();

            State::engine->load(code);
            int correct = 0;
            for (int i = 0; i < 10; i++) {
                alignas(256) char output[256] = {0};
                State::engine->run(inputs[i].data, output, State::max_runtime);
                if ((unsigned char)output[0] == sum[i])
                    correct++;
            }
            return std::to_string(correct) + "/10 correct";
        }
};

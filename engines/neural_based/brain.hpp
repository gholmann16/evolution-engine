#pragma once
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <format>
#include <cmath>

struct Synapse {
    unsigned short input;
    unsigned short output;
    float multiplier;
};

struct LiveSynapse {
    float weight;
    float trace;
    unsigned short output;
};

// must be 768 or above
#define SIZE 4096
#define SYNAPSES 10
#define CAPACITY SIZE * SYNAPSES * 2
#define INPUT_NEURONS 512
#define EXCLUDING (SIZE - INPUT_NEURONS)
#define THRESHOLD 0.7f
#define LEAK 0.9f

class Brain : public Engine {
    private:
        int capacity;
    
    protected:
        int head[SIZE + 1];
        int tail[SIZE + 1];
        struct LiveSynapse * synapses;

        void add_synapse(Synapse add) {
            if (tail[add.input] == head[add.input + 1]) {
                size_t distance = head[SIZE] - tail[add.input]; // move all data including last block with garbage data
                for (int i = add.input + 1; i <= SIZE; i++) {
                    head[i] += SYNAPSES;
                    tail[i] += SYNAPSES;
                }
                if (head[SIZE] > capacity) {
                    capacity += 1000;
                    synapses = (LiveSynapse *)realloc(synapses, capacity * sizeof(LiveSynapse));
                }
                memmove(&synapses[head[add.input + 1]], &synapses[tail[add.input]], distance * sizeof(LiveSynapse));
            }

            synapses[tail[add.input]].weight = add.multiplier;
            synapses[tail[add.input]].output = add.output;
            synapses[tail[add.input]].trace = 0;

            tail[add.input]++;
        }

        void clear() {
            for (int i = 0; i <= SIZE; i++) {
                tail[i] = head[i];
            }
            tail[SIZE] = -1;
        }

        size_t fire(float neuron[SIZE], bool to_fire[SIZE]) {
            size_t fires = 0;

            for (int i = 0; i <= EXCLUDING; i++) {
                if (to_fire[i]) {
                    for (int synapse = head[i]; synapse < tail[i]; synapse++) {
                        // printf("firing %d into %d with %f\t", i, outputs[synapse], weights[synapse]);
                        neuron[synapses[synapse].output] += synapses[synapse].weight;
                        synapses[synapse].trace += neuron[i];
                    }
                    neuron[i] = THRESHOLD - 1.0f;
                    fires += tail[i] - head[i];
                }
            }
            return fires;
        }

        void leak(float neuron[SIZE], bool to_fire[SIZE]) {
            // double before = clock();
            for (int i = INPUT_NEURONS; i < SIZE; i++) {
                neuron[i] *= LEAK;
                to_fire[i] = neuron[i] >= THRESHOLD;
                // printf("%d: fire set to %s because %f is %s %f\t", i, to_fire[i] ? "true" : "false", neuron[i], to_fire[i] ? "greater" : "less than", THRESHOLD);
            }
            // double after = clock();
            // printf("Time %s: %lf\n", "neurons", (after - before) / CLOCKS_PER_SEC);

            // before = clock();
            // for (int i = 0; i < head[SIZE]; i++) {
            //     synapses[i].trace *= LEAK;
            // }
            // after = clock();
            // printf("Time %s: %lf\n", "traces", (after - before) / CLOCKS_PER_SEC);
        }

        void noise(float neuron[SIZE]) {
            return;
            for (int i = 0; i < SIZE; i++) {
                neuron[i] += -0.05 + ((float)rand()) / RAND_MAX * (0.05 - -0.05);
            }
        }

    public:
        void * ancestor_prog() override {
            std::string * new_prog = new std::string();
            for (int i = 0; i < INPUT_NEURONS; i++) {
                Synapse input = {
                    .input = static_cast<unsigned short>(i),
                    .output = static_cast<unsigned short>(i + INPUT_NEURONS),
                    .multiplier = 1.0f,
                }, output = {
                    .input = static_cast<unsigned short>(i + INPUT_NEURONS),
                    .output = static_cast<unsigned short>(EXCLUDING + i),
                    .multiplier = 1.0f,
                };

                new_prog->append(reinterpret_cast<const char*>(&input), sizeof(Synapse));
                new_prog->append(reinterpret_cast<const char*>(&output), sizeof(Synapse));
            }
            return new_prog;
        }

        void load(const void * code) {
            const std::string& code_ref = *reinterpret_cast<const std::string*>(code);
            size_t count = code_ref.size() / sizeof(Synapse);
            const Synapse * data = reinterpret_cast<const Synapse*>(code_ref.data());

            for (size_t i = 0; i < count; i++)
                add_synapse(data[i]);
        }

        void reinforce(float strength) {
            for (int i = 0; i < head[SIZE]; i++) {
                synapses[i].weight += (synapses[i].weight > 0) ? synapses[i].trace * strength : -synapses[i].trace  * strength;
                synapses[i].trace = 0;
            }
        }

        void evolve(const void * parent, void * child, size_t randomness) override {
            const std::string& parent_ref = *reinterpret_cast<const std::string *>(parent);
            std::string& child_ref = *reinterpret_cast<std::string*>(child);
            const Synapse * data = reinterpret_cast<const Synapse*>(parent_ref.data());
            size_t count = parent_ref.size() / sizeof(Synapse);
            child_ref.clear();

            float diff = randomness / 1000.0f;
            int changes = rand() % ((int)std::pow(count, 0.9 - diff) + 1);
            for (size_t i = 0; i < count; i++) {
                int chance = rand() % count;
                struct Synapse addition;
                if (chance < changes) { // add
                    float limit = chance % 4 + 1;
                    if (rand() % 2) {
                        addition = (Synapse) {
                            .input = data[rand() % count].input, // make sure input exists, weight towards active
                            .output = static_cast<unsigned short>(rand() % EXCLUDING + INPUT_NEURONS),
                            .multiplier = -limit + ((float)rand()) / RAND_MAX * (limit - -limit),
                        };
                    }
                    else {
                        addition = (Synapse) {
                            .input = static_cast<unsigned short>(rand() % EXCLUDING),
                            .output = data[rand() % count].output, // make sure input exists, weight towards active
                            .multiplier = -limit + ((float)rand()) / RAND_MAX * (limit - -limit),
                        };
                    }
                    i--; // i-- to run again
                }
                if (chance < changes * 2) // delete
                    continue;
                else // modify
                    addition = data[i];

                switch(rand() % randomness) {
                    case 0:
                    case 4:
                        addition.multiplier *= 0.975f + ((float)rand()) / RAND_MAX * (1.025f - 0.975f);
                        break;
                    case 1:
                    case 5:
                        addition.multiplier *= 0.925f + ((float)rand()) / RAND_MAX * (1.075f - 0.925f);
                        break;
                    case 2:
                    case 6:
                        addition.multiplier *= 0.875f + ((float)rand()) / RAND_MAX * (1.125f - 0.875f);
                        break;
                    case 3:
                    case 7:
                        addition.multiplier *= 0.825f + ((float)rand()) / RAND_MAX * (1.175f - 0.825f);
                        break;
                }
                child_ref.append(reinterpret_cast<const char*>(&addition), sizeof(Synapse));
            }
        }

    public:
        size_t run(char input[256], char output[256], size_t max) override {
            size_t fires = 0;
            float neuron[SIZE] = {0};
            bool to_fire[SIZE] = {0};

            for (int position = 0; position < 64; position++) {
                unsigned char pattern = 128;
                for (int bit = 0; bit < 8; bit++) {
                    if ((unsigned char)input[position] & pattern) {
                        to_fire[position * 8 + bit] = true;
                    }
                    pattern >>= 1;
                }
            }

            // cycle loop, exit when found
            for (int count = 0; count < 10; count++) {
                leak(neuron, to_fire);
                noise(neuron);
                fires += fire(neuron, to_fire);
                if (fires > max)
                    return max;
            }

            // output 247-255
            float max_power = -1.0 / 0.0;
            unsigned short best = 0;
            for (unsigned short on = EXCLUDING; on < SIZE; on++) {
                if (neuron[on] > max_power) {
                    max_power = neuron[on];
                    best = on;
                }
            }

            output[0] = best - EXCLUDING;
            return fires;
        }

        std::string debug(const void * code) override {
            const std::string& code_ref = *reinterpret_cast<const std::string*>(code);
            size_t count = code_ref.size() / sizeof(Synapse);
            const Synapse * data = reinterpret_cast<const Synapse*>(code_ref.data());

            std::string output;
            for (size_t i = 0; i < count; i++) {
                output += std::format("{{{}, {}, {:.9g}}},\n", data[i].input, data[i].output, data[i].multiplier);
            }
            return output;
        }

        std::string compile(const void * code) override {
            return NULL;
        }

        bool equal(const void * first, const void * second) override {
            const std::string& code_ref = *reinterpret_cast<const std::string*>(first);
            size_t count1 = code_ref.size() / sizeof(Synapse);
            // const Synapse * data1 = reinterpret_cast<const Synapse*>(code_ref.data());

            const std::string& next_code_ref = *reinterpret_cast<const std::string*>(second);
            size_t count2 = next_code_ref.size() / sizeof(Synapse);
            // const Synapse * data2 = reinterpret_cast<const Synapse*>(next_code_ref.data());

            if (count1 != count2)
                return false;

            // doesn't check for different orders but like whatever, when even a single different synapses would mean unequal
            // for (size_t i = 0; i < count1; i++) {
            //     if (data1[i] != data2[i])
            //         return false;
            // }

            return true;
        }

        size_t size(const void * code) {
            const std::string& code_ref = *reinterpret_cast<const std::string*>(code);
            return code_ref.size() / sizeof(Synapse);
        }

        void copy_into(const void * parent, void * child) {
            const std::string& parent_ref = *reinterpret_cast<const std::string*>(parent);
            std::string& child_ref = *reinterpret_cast<std::string*>(child);
            child_ref = parent_ref;
        }

        Brain() {
            trainable = true;
            synapses = (struct LiveSynapse *)malloc(sizeof(LiveSynapse) * CAPACITY);
            capacity = CAPACITY;

            for (int i = 0; i <= SIZE; i++) {
                head[i] = i * SYNAPSES;
                tail[i] = i * SYNAPSES;
            }

            tail[SIZE] = -1;
        }

        ~Brain() {
            free(synapses);
        }
};
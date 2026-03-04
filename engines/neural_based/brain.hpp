#pragma once
#include <vector>
#include <cstring>
#include <cstdlib>
#include <format>

struct Synapse {
    unsigned short input;
    unsigned short output;
    float multiplier;
};

// must be 768 or above
#define SIZE 2048
#define SYNAPSES 5
#define CAPACITY SIZE * SYNAPSES * 2
#define INPUT_NEURONS 512
#define EXCLUDING (SIZE - INPUT_NEURONS)
#define THRESHOLD 0.7f
#define LEAK 0.9f

class Brain {
    private:
        int capacity;

    public:
        float * weights;
        unsigned short * outputs;

        int head[SIZE + 1];
        int tail[SIZE + 1];

        size_t size;

        Brain() {
            weights = (float *)malloc(sizeof(float) * CAPACITY);
            outputs = (unsigned short *)malloc(sizeof(unsigned short) * CAPACITY);
            capacity = CAPACITY;
            size = 0;

            for (int i = 0; i <= SIZE; i++) {
                head[i] = i * SYNAPSES;
                tail[i] = i * SYNAPSES;
            }

            tail[SIZE] = -1;

            for (int i = 0; i < INPUT_NEURONS; i++) {
                add_synapse({
                    .input = static_cast<unsigned short>(i),
                    .output = static_cast<unsigned short>(i + INPUT_NEURONS),
                    .multiplier = 1.0,
                });
                add_synapse({
                    .input = static_cast<unsigned short>(i + INPUT_NEURONS),
                    .output = static_cast<unsigned short>(EXCLUDING + i),
                    .multiplier = 1.0,
                });
            }
        }

        ~Brain() {
            free(weights);
            free(outputs);
        }

        void add_synapse(Synapse add) {
            if (tail[add.input] == head[add.input + 1]) {
                size_t distance = head[SIZE] - tail[add.input]; // move all data including last block with garbage data
                for (int i = add.input + 1; i <= SIZE; i++) {
                    head[i] += SYNAPSES;
                    tail[i] += SYNAPSES;
                }
                if (head[SIZE] > capacity) {
                    capacity += 1000;
                    weights = (float *)realloc(weights, capacity * sizeof(float));
                    outputs = (unsigned short *)realloc(outputs, capacity * sizeof(unsigned short));
                }
                memmove(&weights[head[add.input + 1]], &weights[tail[add.input]], distance * sizeof(float));
                memmove(&outputs[head[add.input + 1]], &outputs[tail[add.input]], distance * sizeof(unsigned short));
            }

            int total_synapses = tail[add.input] - head[add.input];
            bool placed = false;

            for (int synapse = 0; synapse < total_synapses; synapse++) {
                if (outputs[head[add.input] + synapse] >= add.output) {
                    memmove(&weights[head[add.input] + synapse + 1], &weights[head[add.input] + synapse], (total_synapses - synapse) * sizeof(float));
                    memmove(&outputs[head[add.input] + synapse + 1], &outputs[head[add.input] + synapse], (total_synapses - synapse) * sizeof(unsigned short));

                    weights[head[add.input] + synapse] = add.multiplier;
                    outputs[head[add.input] + synapse] = add.output;

                    placed = true;
                    break;
                }
            }

            if (!placed) {
                weights[tail[add.input]] = add.multiplier;
                outputs[tail[add.input]] = add.output;
            }

            tail[add.input]++;
            size++;
        }

        void clear() {
            size = 0;
            for (int i = 0; i <= SIZE; i++) {
                tail[i] = head[i];
            }
            tail[SIZE] = -1;
        }

        class Iterator {
            private:
                const Brain * brain;
                unsigned short neuron;
                int synapse;

                void valid_next() {
                    // while the synapse equals the first empty synapse of the neuron (invalid), go to next
                    // when it gets to tail[SIZE], the tail is -1, so synapse it will be "valid"
                    while (synapse == brain->tail[neuron] - brain->head[neuron]) {
                        neuron++;
                        synapse = 0;
                    }
                }

            public:
                Iterator(const Brain* b, int n, int s) : brain(b), neuron(n), synapse(s) {
                    valid_next();
                }

                Synapse operator* () const {
                    return Synapse {
                        .input = neuron,
                        .output = brain->outputs[brain->head[neuron] + synapse],
                        .multiplier = brain->weights[brain->head[neuron] + synapse],
                    };
                }

                bool operator!= (const Iterator& other) const {
                    // checks to make sure it's not on the end neuron aka 1 past the last synapse yet
                    return neuron != other.neuron;
                }

                Iterator& operator++() {
                    synapse++;
                    valid_next();
                    return *this;
                }
        };

        Iterator begin() const {
            return Iterator(this, 0, 0);
        }

        Iterator end() const {
            return Iterator(this, SIZE, 0);
        }
};

class BrainWrapper : public Engine {
    protected:
        size_t fire(float neuron[SIZE], bool to_fire[SIZE], const struct Brain * brain) {
            const float* __restrict weights = brain->weights;
            const unsigned short* __restrict outputs = brain->outputs;
            size_t fires = 0;

            for (int i = 0; i <= EXCLUDING; i++) {
                if (to_fire[i]) {
                    for (int synapse = brain->head[i]; synapse < brain->tail[i]; synapse++) {
                        // printf("firing %d into %d with %f\t", i, outputs[synapse], weights[synapse]);
                        neuron[outputs[synapse]] += weights[synapse];
                    }
                    neuron[i] = THRESHOLD - 1.0;
                    fires += brain->tail[i] - brain->head[i];
                }
            }
            return fires;
        }

        void leak(float neuron[SIZE], bool to_fire[SIZE]) {
            for (int i = INPUT_NEURONS; i < SIZE; i++) {
                neuron[i] *= LEAK;
                to_fire[i] = neuron[i] >= THRESHOLD;
                // printf("%d: fire set to %s because %f is %s %f\t", i, to_fire[i] ? "true" : "false", neuron[i], to_fire[i] ? "greater" : "less than", THRESHOLD);
            }
        }

        void noise(float neuron[SIZE]) {
            return;
            for (int i = 0; i < SIZE; i++) {
                neuron[i] += -0.05 + ((float)rand()) / RAND_MAX * (0.05 - -0.05);
            }
        }

        struct Synapse random_synapse(float limit) {
            return Synapse {
                .input = static_cast<unsigned short>(rand() % EXCLUDING),
                .output = static_cast<unsigned short>(rand() % EXCLUDING + INPUT_NEURONS),
                .multiplier = -limit + ((float)rand()) / RAND_MAX * (limit - -limit),
            };
        }

    public:
        void * ancestor_prog() override {
            return new Brain();
        }

        std::string debug(const void * code) override {
            const struct Brain * brain = reinterpret_cast<const struct Brain *>(code);
            std::string output;
            for (Synapse napse : *brain) {
                output += std::format("{{{}, {}, {:.9g}}},\n", napse.input, napse.output, napse.multiplier);
            }
            return output;
        }

        std::string compile(const void * code) override {
            return NULL;
        }

        bool equal(const void * first, const void * second) override {
            const struct Brain * brain1 = reinterpret_cast<const struct Brain *>(first);
            const struct Brain * brain2 = reinterpret_cast<const struct Brain *>(second);
            int neuron;

            /*
             * Can't use iterator cause there's two
             * Check synapse count discrepancies first because it's faster
             * Reverse order because input neurons are standardized at the beginning
             */
            for (neuron = SIZE - 1; neuron >= 0; neuron--) {
                if (brain1->tail[neuron] - brain1->head[neuron] != brain2->tail[neuron] - brain2->head[neuron]) {
                    return false;
                }
            }

            for (neuron = SIZE - 1; neuron >= 0; neuron--) {
                for (int synapse = 0; synapse < brain1->tail[neuron] - brain1->head[neuron]; synapse++) {
                    if (brain1->outputs[brain1->head[neuron] + synapse] != brain2->outputs[brain2->head[neuron] + synapse]) {
                        return false;
                    }
                }
            }
            return true;
        }

        size_t size(const void * code) {
            const struct Brain * brain1 = reinterpret_cast<const struct Brain *>(code);
            return brain1->size;
        }

        void copy_into(const void * parent, void * child) {
            const struct Brain * brain1 = reinterpret_cast<const struct Brain *>(parent);
            struct Brain * brain2 = reinterpret_cast<struct Brain *>(child);
            brain2->clear();
            for (Synapse napse : *brain1) {
                brain2->add_synapse(napse);
            }
        }
};
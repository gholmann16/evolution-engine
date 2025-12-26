#include <vector>
#include <cstring>
#include <cstdlib>

struct Synapse {
    unsigned short input;
    unsigned short output;
    float multiplier;
};

#define SIZE 256
#define SYNAPSES 10

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
            weights = (float *)malloc(sizeof(float) * 3000);
            outputs = (unsigned short *)malloc(sizeof(unsigned short) * 3000);
            capacity = 3000;
            size = 0;

            for (int i = 0; i <= SIZE; i++) {
                head[i] = i * SYNAPSES;
                tail[i] = i * SYNAPSES;
            }

            tail[SIZE] = -1;
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

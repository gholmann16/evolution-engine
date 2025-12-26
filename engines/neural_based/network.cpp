#include "brain.hpp"
#include <evolver.hpp>
#include <set>
#include <string>
#include <cmath>
#include <format>

#define THRESHOLD 0.7f
#define LEAK 0.9f
#define DRAIN 0.8f

class Network : public Engine {
    private:
        struct Synapse random_synapse(float limit) {
            return Synapse {
                .input = static_cast<unsigned short>(rand() % 247),
                .output = static_cast<unsigned short>(rand() % 229 + 27),
                .multiplier = -limit + ((float)rand()) / RAND_MAX * (limit - -limit),
            };
        }

    public:
        void * ancestor_prog() override {
            struct Brain * brain = new Brain();

            Synapse connections[] = {
                {0, 100, 1.0},
                {3, 103, 1.0},
                {6, 106, 1.0},
                {9, 109, 1.0},
                {12, 112, 1.0},
                {15, 115, 1.0},
                {18, 118, 1.0},
                {21, 121, 1.0},
                {24, 124, 1.0},
                {100, 247, 1.0},
                {103, 248, 1.0},
                {106, 249, 1.0},
                {109, 250, 1.0},
                {112, 251, 1.0},
                {115, 252, 1.0},
                {118, 253, 1.0},
                {121, 254, 1.0},
                {124, 255, 1.0},
            };

            for (Synapse napse : connections) {
                brain->add_synapse(napse);
            }
            return brain;
        }

        void evolve(const void * parent, void * child, size_t randomness) override {
            const struct Brain * parent_brain = reinterpret_cast<const struct Brain *>(parent);
            struct Brain * child_brain = reinterpret_cast<struct Brain *>(child);
            size_t amount = parent_brain->size;
            child_brain->clear();

            /*
             * probably would be faster to full copy and then modify from there, but then i might have to start doing memmoves
             * i could just pop only the last synapse per neuron but that would hurt the evolution and honestly generation takes no time
             */
            for (Synapse napse : *parent_brain) {
                switch(rand() % randomness) {
                    case 0: // 1% chance you delete
                        break;
                    case 1: // 6% chance you change the multiplier
                    case 2:
                    case 3:
                    case 4:
                    case 5:
                        napse.multiplier *= 0.975f + ((float)rand()) / RAND_MAX * (1.025f - 0.975f);
                        child_brain->add_synapse(napse);
                        break;
                    case 6:
                        napse.multiplier *= 0.875f + ((float)rand()) / RAND_MAX * (1.125f - 0.875f);
                        child_brain->add_synapse(napse);
                        break;
                    default: // 95 % chance you do nothing (slightly more because additions go here after)
                        child_brain->add_synapse(napse);
                        break;
                }
            }

            float diff = randomness / 1000.0f;
            int additions = rand() % ((int)std::pow(amount, 0.9 - diff) + 1);
            for (int j = 0; j < additions; j++) {
                float limit = j % 4 + 1;
                child_brain->add_synapse({
                    .input = static_cast<unsigned short>(rand() % 247),
                    .output = static_cast<unsigned short>(rand() % 229 + 27),
                    .multiplier = -limit + ((float)rand()) / RAND_MAX * (limit - -limit),
                });
            }
        }

        // uses graphviz to output https://dreampuf.github.io/GraphvizOnline/
        std::string debug(const void * code) override {
            const struct Brain * brain = reinterpret_cast<const struct Brain *>(code);
            std::string graphviz;
            for (Synapse napse : *brain) {
                graphviz += std::format("{} -> {} [color=\"{}\", penwidth={}, label=\"{}\"];\n",
                    napse.input, napse.output, (napse.multiplier > 0) ? "green" : "red", std::fabs(napse.multiplier), napse.multiplier);
            }
            return graphviz;
        }

        std::string compile(const void * code) override {
            return NULL;
        }

        size_t run(const void * code, char input[256], char output[256], size_t max) override {
            const struct Brain * brain = reinterpret_cast<const struct Brain *>(code);
            const float* __restrict weights = brain->weights;
            const unsigned short* __restrict outputs = brain->outputs;

            float neuron[256] = {0};
            unsigned short current_neuron = -1;
            char options[3] = {' ', 'O', 'X'};

            // cycle loop, exit when found
            for (int count = 0; count < 10; count++) {
                for (int position = 0; position < 9; position++)
                    for (int offset = 0; offset < 3; offset++)
                        if(input[position] == options[offset])
                            for (int synapse = brain->head[position * 3 + offset]; synapse < brain->tail[position * 3 + offset]; synapse++)
                                neuron[outputs[synapse]] += weights[synapse];

                for (current_neuron = 27; current_neuron < 247; current_neuron++) {
                    if (neuron[current_neuron] >= THRESHOLD) {
                        for (int synapse = brain->head[current_neuron]; synapse < brain->tail[current_neuron]; synapse++) {
                            neuron[outputs[synapse]] += weights[synapse];
                        }
                        neuron[current_neuron] = 0;
                    }
                    else
                        neuron[current_neuron] *= LEAK;
                }

                // deplete battery / output
                for (current_neuron = 247; current_neuron < 256; current_neuron++) {
                    neuron[current_neuron] *= DRAIN;
                }
            }

            // output 247-255
            float max_power = 0.0;
            unsigned short best = 0;
            for (unsigned short on = 247; on < 256; on++) {
                // printf("%d: %f\n", on, neuron[on]);
                if (neuron[on] > max_power) {
                    max_power = neuron[on];
                    best = on;
                }
            }

            output[0] = best - 247;
            return 0;
        }

        bool equal(const void * first, const void * second) override {
            const struct Brain * brain1 = reinterpret_cast<const struct Brain *>(first);
            const struct Brain * brain2 = reinterpret_cast<const struct Brain *>(second);
            int neuron;

            for (neuron = SIZE - 1; neuron >= 0; neuron++) {
                if (brain1->tail[neuron] - brain1->head[neuron] != brain2->tail[neuron] - brain2->head[neuron]) {
                    return false;
                }
            }

            for (neuron = SIZE - 1; neuron >= 0; neuron++) {
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
};

Engine * create_network() {
    return new Network();
}

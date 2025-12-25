#include "synapse.hpp"
#include <evolver.hpp>
#include <set>
#include <string>
#include <cmath>
#include <format>

#define THRESHOLD 0.7f
#define LEAK 0.9f;
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
        std::string ancestor_prog() override {
            // must be in input order
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
            return std::string(reinterpret_cast<const char*>(connections), sizeof(connections));
        }

        void evolve(const std::string& parent, std::string &child, size_t randomness) override {
            const struct Synapse * connections = reinterpret_cast<const struct Synapse *>(parent.data());
            size_t amount = parent.size() / sizeof(struct Synapse);
            child.clear();
            struct Synapse temp;

            for (size_t i = 0; i < amount; i++) {
                switch(rand() % randomness) {
                    case 0: // 1% chance you delete
                        break;
                    case 1: // 6% chance you change the multiplier
                    case 2:
                    case 3:
                    case 4:
                    case 5:
                        temp = connections[i];
                        temp.multiplier *= 0.975f + ((float)rand()) / RAND_MAX * (1.025f - 0.975f);
                        child.append(reinterpret_cast<const char*>(&temp), sizeof(Synapse));
                        break;
                    case 6:
                        temp = connections[i];
                        temp.multiplier *= 0.875f + ((float)rand()) / RAND_MAX * (1.125f - 0.875f);
                        child.append(reinterpret_cast<const char*>(&temp), sizeof(Synapse));
                        break;
                    default: // 95 % chance you do nothing (slightly more because additions go here after)
                        child.append(reinterpret_cast<const char*>(&connections[i]), sizeof(Synapse));
                        break;
                }
            }

            float diff = randomness / 1000.0f;
            int additions = rand() % ((int)std::pow(amount, 0.9 - diff) + 1);
            size_t old_len = child.size();
            for (int j = 0; j < additions; j++) {
                temp = random_synapse((j % 4) + 1);
                child.append(reinterpret_cast<const char*>(&temp), sizeof(Synapse));
            }
            Synapse * begin = reinterpret_cast<Synapse*>(&child[0]);
            Synapse * pivot = reinterpret_cast<Synapse*>(&child[old_len]);
            Synapse * end = reinterpret_cast<Synapse*>(&child[child.size()]);

            // sort n merge in place
            std::sort(pivot, end, [](const Synapse& a, const Synapse& b) {
                return a.input < b.input;
            });

            std::inplace_merge(begin, pivot, end, [](const Synapse& a, const Synapse& b) {
                return a.input < b.input;
            });
        }

        // uses graphviz to output https://dreampuf.github.io/GraphvizOnline/
        std::string debug(const std::string& code) override {
            const struct Synapse * connections = reinterpret_cast<const struct Synapse *>(code.data());
            size_t amount = code.size() / sizeof(struct Synapse);
            std::string graphviz;
            for (size_t i = 0; i < amount; i++) {
                graphviz += std::format("{} -> {} [color=\"{}\", penwidth={}, label=\"{}\"];\n",
                    connections[i].input, connections[i].output, (connections[i].multiplier > 0) ? "green" : "red", std::fabs(connections[i].multiplier), connections[i].multiplier);
            }
            return graphviz;
        }

        std::string compile(const std::string& code) override {
            return NULL;
        }

        size_t run(const std::string& code, char input[256], char output[256], size_t max) override {
            const struct Synapse * connections = reinterpret_cast<const struct Synapse *>(code.data());
            size_t amount = code.size() / sizeof(struct Synapse);
            float neuron[256] = {0};
            unsigned short current_id = -1;
            char options[3] = {' ', 'O', 'X'};

            // cycle loop, exit when found
            for (int count = 0; count < 10; count++) {
                size_t current_synapse = 0;
                for (current_id = 0; current_id < 27; current_id++) {
                    bool fire = input[current_id / 3] == options[current_id % 3];
                    while(connections[current_synapse].input == current_id && current_synapse < amount) {
                        // if has to be within, because it has to cycle through regardless
                        if (fire)
                            neuron[connections[current_synapse].output] += connections[current_synapse].multiplier;

                        // printf("checking if %d at %d is equal to %d condition %s\n", input[current_id / 3], current_id, options[current_id % 3], input[current_id] == options[current_id % 3] ? "true" : "false");
                        // printf("running %d -> %d now @ %f\n", current_id, connections[current_synapse].output, neuron[connections[current_synapse].output]);
                        current_synapse++;
                    }
                }
    
                for (current_id = 27; current_id < 247; current_id++) {
                    bool fire = neuron[current_id] >= THRESHOLD;
                    while(connections[current_synapse].input == current_id && current_synapse < amount) {
                        // if has to be within, because it has to cycle through regardless
                        if (fire)
                            neuron[connections[current_synapse].output] += connections[current_synapse].multiplier;
                        current_synapse++;
                    }
                    if (fire)
                        neuron[current_id] = 0;
                    else
                        neuron[current_id] *= LEAK; 
                }

                // deplete battery / output
                for (current_id = 247; current_id < 256; current_id++) {
                    neuron[current_id] *= DRAIN;
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

        bool equal(const std::string& first, const std::string& second) override {
            const struct Synapse * first_connections = reinterpret_cast<const struct Synapse *>(first.data());
            size_t first_amount = first.size() / sizeof(struct Synapse);
            const struct Synapse * second_connections = reinterpret_cast<const struct Synapse *>(second.data());
            size_t second_amount = second.size() / sizeof(struct Synapse);

            if (first_amount != second_amount)
                return false;

            // backwards is probably faster cause most random additions will be at the end
            for (size_t i = first_amount; i-- > 0;) {
                if (first_connections[i].input != second_connections[i].input || first_connections[i].output != second_connections[i].output)
                    return false;
            }
            return true;
        }
};

Engine * create_network() {
    return new Network();
}

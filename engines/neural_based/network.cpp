#include "neuron.hpp"
#include <evolver.hpp>
#include <set>
#include <string>
#include <cmath>
#include <format>
#define STARTING 20

class Network : public Engine {
    private:
        void fire(const Neuron& nur) {
            for (Output connect : nur.outs)
                (*(connect.out)).power += connect.multiplier;
            nur.power = 0.0;
        }
        // named flood cause the numbers kind of flood through the brain
        void cycle(std::set<Neuron>& brain) {
            for (const Neuron& nur : brain) {
                if (nur.power == 0.0)
                    continue;
                else if (nur.power >= 0.7 && nur.outs.size() != 0) {
                    fire(nur);
                }
                else {
                    // printf("%d lowered from %f to %f\n", nur.id, nur.power, nur.power * 0.9);
                    nur.power *= 0.9;
                }
            }
        }

        struct Synapse random_synapse() {
            return Synapse {
                .input = static_cast<unsigned char>(rand() % 256),
                .output = static_cast<unsigned char>(rand() % 256),
                .multiplier = -4.0f + ((float)rand()) / RAND_MAX * (4.0f - -4.0f),
            };
        }

    public:
        std::string ancestor_prog() override {
            Synapse connections[] = {
                {0, 29, 1.0},
                {3, 30, 1.0},
                {6, 31, 1.0},
                {9, 32, 1.0},
                {12, 33, 1.0},
                {15, 34, 1.0},
                {18, 35, 1.0},
                {21, 36, 1.0},
                {24, 37, 1.0},
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
                    case 0: // 1 % chance you remove code
                        break;
                    case 1: // 3 % chance you add code
                    case 2:
                    case 3:
                        i--;
                        temp = random_synapse();
                        child.append(reinterpret_cast<const char*>(&temp), sizeof(Synapse));
                        break;
                    case 4: // 6% chance you change the multiplier
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                        temp = connections[i];
                        temp.multiplier *= 0.975f + ((float)rand()) / RAND_MAX * (1.025f - 0.975f);
                        child.append(reinterpret_cast<const char*>(&temp), sizeof(Synapse));
                        break;
                    case 10:
                    case 11:
                        temp = connections[i];
                        temp.multiplier *= -0.9f + ((float)rand()) / RAND_MAX * (1.1f - 0.9f);
                        child.append(reinterpret_cast<const char*>(&temp), sizeof(Synapse));
                        break;
                    default: // 95 % chance you do nothing (slightly more because additions go here after)
                        child.append(reinterpret_cast<const char*>(&connections[i]), sizeof(Synapse));
                        break;
                }
            }
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
            std::set<Neuron> brain = std::set<Neuron>();

            // wire/populate brain
            Neuron key = {0};
            for (size_t n = 0; n < amount; n++) {
                auto [input_neuron, input_ignored] = brain.emplace(Neuron {
                    .id = connections[n].input,
                    .power = 0.0,
                    .outs = std::vector<Output>(),
                });

                auto [output_neuron, output_ignored] = brain.emplace(Neuron {
                    .id = connections[n].output,
                    .power = 0.0,
                    .outs = std::vector<Output>(),
                });

                input_neuron->outs.push_back(Output {
                    .out = &(*output_neuron),
                    .multiplier = connections[n].multiplier,
                });
            }

            // cycle loop, exit when found
            for (int count = 0; count < 10; count++) {
                for (int i = 0; i < 9; i++) {
                    int multiplier = 0;
                    switch(input[i]) {
                        default:
                            puts("Something has gone wrong");
                            exit(-1);
                        case 'X':
                            multiplier++;
                        case 'O':
                            multiplier++;
                        case ' ':
                            break;
                    }
                    key.id = i*3 + multiplier;
                    auto input_neuron = brain.find(key);
                    if (input_neuron != brain.end()) {
                        // printf("debug power %d\n", nur.power);
                        fire(*input_neuron);
                    }
                }
                // input 27-28 (first or second)
                int num = 0;
                for (int i = 0; i < 9; i++)
                    num += input[i] == ' ' ? 0 : 1;
                key.id = 27 + num % 2; // if number is odd, you are X, 37
                auto player_input = brain.find(key);
                if (player_input != brain.end())
                    fire(*player_input);
    
                cycle(brain);
            }

            // output 27-35
            float max_power = 0.0;
            unsigned char best = 0;
            for (unsigned char on = 29; on < 38; on++) {
                key.id = on;
                auto output_neuron = brain.find(key);
                // if (output_neuron != brain.end()) {
                //     printf("%d: %f\n", output_neuron->id, output_neuron->power);
                // }
                if (output_neuron != brain.end() && output_neuron->power > max_power) {
                    max_power = output_neuron->power;
                    best = on;
                }
            }
            
            output[0] = best - 29;
            return 0;
        }

        bool equal(const std::string& first, const std::string& second) override {
            const struct Synapse * first_connections = reinterpret_cast<const struct Synapse *>(first.data());
            size_t first_amount = first.size() / sizeof(struct Synapse);
            const struct Synapse * second_connections = reinterpret_cast<const struct Synapse *>(second.data());
            size_t second_amount = second.size() / sizeof(struct Synapse);

            if (first_amount != second_amount)
                return false;

            for (size_t i = 0; i < first_amount; i++) {
                if (first_connections[i].input != second_connections[i].input || first_connections[i].output != second_connections[i].output)
                    return false;
                float difference = first_connections[i].multiplier - second_connections[i].multiplier;
                if (difference > 0.05f && difference < -0.05f) {
                    return false;
                }
            }
            return true;
        }
};

Engine * create_network() {
    return new Network();
}

#include "brain.hpp"
#include <set>
#include <string>
#include <cmath>

class Network : public BrainWrapper {
    private:
        size_t fires;

    public:
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
                child_brain->add_synapse(random_synapse(limit));
            }
        }

        size_t run(const void * code, char input[256], char output[256], size_t max) override {
            fires = 0;
            const struct Brain * brain = reinterpret_cast<const struct Brain *>(code);
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
                fires += fire(neuron, to_fire, brain);
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
};

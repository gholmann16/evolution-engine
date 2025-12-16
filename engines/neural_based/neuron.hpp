#include <vector>

#pragma pack(push, 1)
struct Synapse {
    unsigned char input;
    unsigned char output;
    float multiplier;
};
#pragma pack(pop)

struct Output {
    const struct Neuron * out;
    float multiplier;
};

struct Neuron {
    unsigned char id;
    mutable float power;
    mutable std::vector<Output> outs;
    bool operator<(const Neuron& other) const {
        return id < other.id;
    }
};

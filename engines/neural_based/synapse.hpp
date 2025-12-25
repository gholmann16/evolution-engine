#include <vector>

#pragma pack(push, 1)
struct Synapse {
    unsigned short input;
    unsigned short output;
    float multiplier;
};
#pragma pack(pop)

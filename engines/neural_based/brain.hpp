#pragma once
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <format>
#include <cmath>
#include <algorithm>

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

#define SIZE 8192                 // 2048 in + 4096 hidden + 2048 out
#define SYNAPSES 10
#define CAPACITY (SIZE * SYNAPSES * 2)
#define INPUT_NEURONS 2048        // 256 bytes * 8 bits, uses the whole input buffer
#define OUTPUT_NEURONS 2048       // 256 bytes * 8 bits, bit-packed into the whole output buffer
#define EXCLUDING (SIZE - OUTPUT_NEURONS)   // neurons 0..EXCLUDING propagate (inputs + hidden)
#define THRESHOLD 0.7f
#define LEAK 0.9f
#define EQUAL_BAR 0.90f           // >= 90% shared wiring counts as "the same creature"
#define ANCESTOR_SYNAPSES (SIZE * 2)
#define RUNS 30

static_assert(SIZE >= INPUT_NEURONS + OUTPUT_NEURONS, "no room for hidden neurons");
static_assert(SIZE <= 65536, "neuron indices must fit unsigned short");
static_assert(INPUT_NEURONS == 256 * 8 && OUTPUT_NEURONS == 256 * 8,
              "run() maps the full 256-byte buffers bit-per-neuron");

class Brain : public Engine {
    private:
        int capacity;

        // xorshift64: ~3.4x faster evolve than glibc rand() (which also takes
        // a lock). Reseeded from the rand() stream at the top of evolve(), so
        // srand(seed + runs) fully determines every child -- your seeds
        // reproduce runs again. (Seeds from the old rand()-only engine map to
        // different runs; they're only comparable within one engine version.)
        uint64_t rng_state = 0x9E3779B97F4A7C15ull;
        inline uint64_t rng() {
            uint64_t x = rng_state;
            x ^= x << 13; x ^= x >> 7; x ^= x << 17;
            return rng_state = x;
        }
        inline uint32_t rng_below(uint32_t n) { return (uint32_t)(rng() % n); }
        inline float rng_unit() { return (float)(rng() >> 40) * (1.0f / 16777216.0f); } // [0,1)
        inline void rng_reseed_from_rand() {
            // |1 avoids the all-zero state, which is xorshift's fixed point
            rng_state = ((((uint64_t)rand()) << 32) ^ (uint64_t)rand()) | 1;
        }

        // scratch for equal(); reused across calls so no allocation in steady state
        std::vector<uint32_t> eq_keys;
        std::vector<uint32_t> eq_cnt;

        void reserve(int needed) {
            if (needed <= capacity) return;
            while (capacity < needed) capacity *= 2;   // geometric, not +1000
            synapses = (LiveSynapse *)realloc(synapses, capacity * sizeof(LiveSynapse));
        }

    protected:
        int head[SIZE + 1];   // head[i]..tail[i] is neuron i's synapse row
        int tail[SIZE + 1];   // after load(): exact-fit CSR, tail[i] == head[i+1], head[SIZE] == count
        struct LiveSynapse * synapses;

        // Kept for incremental insertion. Note: O(total) shift per overflow —
        // for whole genomes always prefer load(), which is O(n).
        void add_synapse(Synapse add) {
            if (tail[add.input] == head[add.input + 1]) {
                size_t distance = head[SIZE] - tail[add.input];
                for (int i = add.input + 1; i <= SIZE; i++) {
                    head[i] += SYNAPSES;
                    tail[i] += SYNAPSES;
                }
                reserve(head[SIZE]);
                memmove(&synapses[head[add.input + 1]], &synapses[tail[add.input]], distance * sizeof(LiveSynapse));
            }
            synapses[tail[add.input]].weight = add.multiplier;
            synapses[tail[add.input]].output = add.output;
            synapses[tail[add.input]].trace = 0;
            tail[add.input]++;
        }

        void clear() {
            head[SIZE] = 0;   // load() rebuilds the whole layout, so clearing is O(1)
        }

        size_t fire(float neuron[SIZE], bool to_fire[SIZE]) {
            size_t fires = 0;
            for (int i = 0; i < EXCLUDING; i++) {
                if (to_fire[i]) {
                    const int h = head[i], t = tail[i];
                    for (int synapse = h; synapse < t; synapse++) {
                        neuron[synapses[synapse].output] += synapses[synapse].weight;
                        synapses[synapse].trace += neuron[i];
                    }
                    neuron[i] = THRESHOLD - 1.0f;
                    fires += t - h;
                }
            }
            return fires;
        }

        void leak(float * __restrict neuron, bool * __restrict to_fire) {
            for (int i = INPUT_NEURONS; i < SIZE; i++) {
                neuron[i] *= LEAK;
                to_fire[i] = neuron[i] >= THRESHOLD;
            }
        }

        void noise(float neuron[SIZE]) {
            return;
            for (int i = 0; i < SIZE; i++) {
                neuron[i] += -0.05f + rng_unit() * 0.1f;
            }
        }

    public:
        // "Super random" wiring, with one guardrail: every input neuron gets
        // at least one outgoing synapse. Purely random sparse wiring leaves
        // many inputs disconnected, and a dead input gives evolution zero
        // gradient to ever discover it. The guardrail costs nothing and only
        // adds randomness (the target and weight are still random). Delete
        // the first loop if you want pure chaos.
        // Uses rand() directly: called once at startup, so speed is
        // irrelevant and it stays on your srand() stream for determinism.
        void * ancestor_prog() override {
            std::string * new_prog = new std::string();
            new_prog->reserve(ANCESTOR_SYNAPSES * sizeof(Synapse));

            for (int i = 0; i < INPUT_NEURONS; i++) {
                Synapse s = {
                    .input = static_cast<unsigned short>(i),
                    .output = static_cast<unsigned short>(INPUT_NEURONS + rand() % (SIZE - INPUT_NEURONS)),
                    .multiplier = -2.0f + 4.0f * (float)rand() / RAND_MAX,
                };
                new_prog->append(reinterpret_cast<const char*>(&s), sizeof(Synapse));
            }
            for (int i = INPUT_NEURONS; i < ANCESTOR_SYNAPSES; i++) {
                Synapse s = {
                    .input = static_cast<unsigned short>(rand() % EXCLUDING),                       // any propagating neuron
                    .output = static_cast<unsigned short>(INPUT_NEURONS + rand() % (SIZE - INPUT_NEURONS)), // any non-input neuron
                    .multiplier = -2.0f + 4.0f * (float)rand() / RAND_MAX,
                };
                new_prog->append(reinterpret_cast<const char*>(&s), sizeof(Synapse));
            }
            return new_prog;
        }

        // Two-pass counting-sort build: O(n) for any genome. Replaces the
        // add_synapse path, which shifts all downstream data every time a
        // 10-slot block overflows (O(n * SIZE) on evolved genomes).
        void load(const void * code) {
            const std::string& code_ref = *reinterpret_cast<const std::string*>(code);
            size_t count = code_ref.size() / sizeof(Synapse);
            const Synapse * data = reinterpret_cast<const Synapse*>(code_ref.data());

            reserve((int)count);

            // pass 1: count synapses per input neuron, prefix-sum in place.
            // The last prefix-sum step leaves head[SIZE] == count (every
            // synapse lands in exactly one of head[1..SIZE], and the sweep
            // folds them all into the final slot) -- reinforce depends on it.
            memset(head, 0, sizeof(head));
            for (size_t i = 0; i < count; i++)
                head[data[i].input + 1]++;          // count into next slot
            for (int i = 0; i < SIZE; i++) {
                head[i + 1] += head[i];             // prefix sum in place
                tail[i] = head[i];                  // cursor for pass 2
            }

            // pass 2: place
            for (size_t i = 0; i < count; i++) {
                int at = tail[data[i].input]++;
                synapses[at].weight = data[i].multiplier;
                synapses[at].output = data[i].output;
                synapses[at].trace = 0;
            }
        }

        // Only touches real synapses (head[SIZE] == exact count after load).
        void reinforce(float strength) {
            const int n = head[SIZE];
            for (int i = 0; i < n; i++) {
                synapses[i].weight += (synapses[i].weight > 0) ? synapses[i].trace * strength : -synapses[i].trace * strength;
                synapses[i].trace = 0;
            }
        }

        void evolve(const void * parent, void * child, size_t randomness) override {
            rng_reseed_from_rand();   // ties this child to the srand() stream

            const std::string& parent_ref = *reinterpret_cast<const std::string *>(parent);
            std::string& child_ref = *reinterpret_cast<std::string*>(child);
            const Synapse * data = reinterpret_cast<const Synapse*>(parent_ref.data());
            size_t count = parent_ref.size() / sizeof(Synapse);
            child_ref.clear();
            child_ref.reserve(parent_ref.size() + parent_ref.size() / 8);   // avoid regrowth on append

            float diff = randomness / 1000.0f;
            int changes = rng_below((uint32_t)std::pow(count, 0.9 - diff) + 1);
            for (size_t i = 0; i < count; i++) {
                int chance = (int)rng_below((uint32_t)count);
                struct Synapse addition;
                if (chance < changes) { // add
                    float limit = chance % 4 + 1;
                    if (rng() & 1) {
                        addition = (Synapse) {
                            .input = data[rng_below((uint32_t)count)].input, // make sure input exists, weight towards active
                            .output = static_cast<unsigned short>(INPUT_NEURONS + rng_below(SIZE - INPUT_NEURONS)),
                            .multiplier = -limit + rng_unit() * (2 * limit),
                        };
                    }
                    else {
                        addition = (Synapse) {
                            .input = static_cast<unsigned short>(rng_below(EXCLUDING)),
                            .output = data[rng_below((uint32_t)count)].output, // make sure output exists, weight towards active
                            .multiplier = -limit + rng_unit() * (2 * limit),
                        };
                    }
                    i--; // i-- to run again
                }
                if (chance < changes * 2) // delete
                    continue;
                else // modify
                    addition = data[i];

                switch(rng_below((uint32_t)randomness)) {
                    case 0: case 4: addition.multiplier *= 0.975f + rng_unit() * 0.05f; break;
                    case 1: case 5: addition.multiplier *= 0.925f + rng_unit() * 0.15f; break;
                    case 2: case 6: addition.multiplier *= 0.875f + rng_unit() * 0.25f; break;
                    case 3: case 7: addition.multiplier *= 0.825f + rng_unit() * 0.35f; break;
                }
                child_ref.append(reinterpret_cast<const char*>(&addition), sizeof(Synapse));
            }
        }

    public:
        size_t run(char input[256], char output[256], size_t max) override {
            size_t fires = 0;
            float neuron[SIZE] = {0};
            bool to_fire[SIZE] = {0};

            // all 256 input bytes -> 2048 input neurons, MSB first
            for (int position = 0; position < 256; position++) {
                unsigned char byte = (unsigned char)input[position];
                for (int bit = 0; bit < 8; bit++)
                    to_fire[position * 8 + bit] = (byte >> (7 - bit)) & 1;
            }

            for (int count = 0; count < RUNS; count++) {
                leak(neuron, to_fire);
                noise(neuron);
                fires += fire(neuron, to_fire);
                if (fires > max)
                    return max;   // note: output buffer left untouched, as before
            }

            // 2048 output neurons -> all 256 output bytes, MSB first,
            // mirroring the input encoding. Bit set = neuron at/above
            // threshold. (Replaces the old argmax -> single char, which
            // silently wrapped once there were more than 256 output neurons.)
            for (int position = 0; position < 256; position++) {
                unsigned char byte = 0;
                for (int bit = 0; bit < 8; bit++)
                    if (neuron[EXCLUDING + position * 8 + bit] >= THRESHOLD)
                        byte |= 128 >> bit;
                output[position] = (char)byte;
            }
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
            return "";   // was `return NULL` -- UB when constructing std::string
        }

        // Fuzzy DNA equality: two genomes are "equal" when >= EQUAL_BAR of
        // their wiring (multiset of input->output pairs) is shared, measured
        // against the LARGER genome. Weights are deliberately ignored: your
        // mutation scales nearly every weight every generation, so any
        // weight-sensitive comparison would declare everything unique and
        // the diversity mechanism in Squarelite::sort() would never trigger.
        // Side effect you want anyway: hall-of-fame repetition counting now
        // treats weight-drifted rediscoveries of the same wiring as repeats.
        // Cost: O(n) per call via a reused open-addressing table.
        bool equal(const void * first, const void * second) override {
            const std::string& A = *reinterpret_cast<const std::string*>(first);
            const std::string& B = *reinterpret_cast<const std::string*>(second);
            const size_t n1 = A.size() / sizeof(Synapse);
            const size_t n2 = B.size() / sizeof(Synapse);
            if (n1 == 0 || n2 == 0)
                return n1 == n2;

            const size_t mx = std::max(n1, n2);
            // matches can't exceed the smaller genome, so a size gap alone
            // can break the bar -- this rejects most pairs with zero work
            if ((float)std::min(n1, n2) < EQUAL_BAR * (float)mx)
                return false;

            const Synapse * a = reinterpret_cast<const Synapse*>(A.data());
            const Synapse * b = reinterpret_cast<const Synapse*>(B.data());

            size_t cap = 16;
            while (cap < n1 * 2) cap <<= 1;
            const size_t mask = cap - 1;
            const uint32_t EMPTY = 0xFFFFFFFFu;   // input==output==0xFFFF impossible: SIZE < 0xFFFF
            eq_keys.assign(cap, EMPTY);
            eq_cnt.assign(cap, 0);

            auto probe = [&](uint32_t key) -> size_t {
                size_t s = (key * 2654435761u) & mask;
                while (eq_keys[s] != EMPTY && eq_keys[s] != key)
                    s = (s + 1) & mask;
                return s;
            };

            for (size_t i = 0; i < n1; i++) {
                const uint32_t key = ((uint32_t)a[i].input << 16) | a[i].output;
                const size_t s = probe(key);
                eq_keys[s] = key;
                eq_cnt[s]++;
            }
            size_t matches = 0;
            for (size_t i = 0; i < n2; i++) {
                const uint32_t key = ((uint32_t)b[i].input << 16) | b[i].output;
                const size_t s = probe(key);
                if (eq_keys[s] == key && eq_cnt[s] > 0) {
                    eq_cnt[s]--;
                    matches++;
                }
            }
            return (float)matches >= EQUAL_BAR * (float)mx;
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
            capacity = CAPACITY;
            synapses = (struct LiveSynapse *)malloc(sizeof(LiveSynapse) * capacity);
            memset(head, 0, sizeof(head));
            memset(tail, 0, sizeof(tail));
        }

        ~Brain() {
            free(synapses);
        }
};

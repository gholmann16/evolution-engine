#pragma once
#include <stddef.h>
#include <memory>
#include <map>
#include <string>

struct Program {
    void * code;
    size_t score;
};

class Engine {
    public:
        virtual ~Engine() = default;
        bool trainable = false;
        // Index into engine_names[]/make_engine(), stamped on by
        // make_engine() at construction time. Lets clone_engine() rebuild
        // "another one of whatever this is" without keeping a registry of
        // live instances around just to compare pointers against.
        int factory_id = -1;

        // input read only, output should be non freeable memory, too many allocs otherwise. Return looptime/runtime
        // virtual size_t train(void * code, char input[256], char output[256], size_t max) = 0;
        virtual void load(const void * code) = 0;
        virtual size_t run(char input[256], char output[256], size_t max) = 0;

        // Evolving
        virtual void * ancestor_prog() = 0;
        virtual void evolve(const void * parent, void * child, size_t randomness) = 0;

        // Utility
        virtual std::string debug(const void * code) = 0; // Should be an allocated string
        // Same as debug(), but with irrelevant/unreachable structure pruned
        // where that's meaningful (e.g. dead synapses in the neural engine).
        // Defaults to the raw debug() for engines with no such concept.
        virtual std::string clean_debug(const void * code) { return debug(code); }
        virtual std::string compile(const void * code) = 0; // Allocated string ready to be passed to assembler
        virtual bool equal(const void * first, const void * second) = 0;
        virtual size_t size(const void * code) = 0;
        virtual void copy_into(const void * parent, void * child) = 0;
};

extern const char * engine_names[];
extern int num_engines;
// Builds a fresh instance of engine type `id` (an index into
// engine_names[], e.g. from the CLI/GUI's engine picker). engines.cpp is
// the one place that knows every concrete engine type, so construction
// lives there instead of a clone() override per class -- and nothing keeps
// a standing instance of each type around, so callers only ever pay for
// the ones they actually use.
Engine * make_engine(int id);
// Fresh instance of e's concrete type (no shared state with e). Needed so
// the scoring thread pool can hand each worker its own engine to
// load()/run() on -- load() mutates per-instance buffers (CSR arrays, JIT'd
// machine code, ...) that aren't safe to share across threads.
Engine * clone_engine(const Engine * e);

class Test {
    public:
        virtual ~Test() = default;
        virtual size_t score(const void * code) const = 0;
        // Optional human-readable rendering of what this code produces (e.g.
        // the raw output string for Output). Empty string means "not shown".
        virtual std::string display(const void * code) const { return ""; }
        // Optional: fills `input` with the same input this test scores
        // against, so a visualization (e.g. the neural node view) can trace
        // a genome's actual response instead of an arbitrary placeholder.
        // Default: leave it as the caller zeroed it -- fine for tests whose
        // input is randomized/adversarial rather than one fixed pattern.
        virtual void reference_input(char input[256]) const { }
};

extern Test * tests[];
extern const char * test_names[];
extern int num_tests;

class Evolver {
    public:
        virtual ~Evolver() = default;

        inline static bool compare_ratings(const Program& first, const Program &second) {
            return first.score < second.score;
        }

        virtual void evolve() const = 0;
        virtual void score_all() const = 0;
        virtual void sort() const = 0;
};

extern Evolver * evolvers[];
extern const char * evolver_names[];
extern int num_evolvers;

namespace State {
    inline struct Program * children;
    inline size_t total_creatures = 10000;

    inline void ** hall_of_fame;
    inline size_t total_famers = 100;

    inline Test * test = nullptr;
    // thread_local: the scoring thread pool (see evolvers/standard.hpp) gives
    // each worker its own clone_engine() result and points that worker's
    // engine/comp at it, without touching the value the main thread sees.
    inline thread_local Engine * engine = nullptr;
    inline thread_local Engine * comp = nullptr;
    inline Evolver * evolver = nullptr;

    inline size_t def_rand = 250;
    inline bool verbose = false;
    inline size_t seed = 0;

    inline size_t repetitions = 0;
    inline size_t runs = 0;

    inline const char * output = "data.txt";
}

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

// Scores State::children[begin..total) using the same worker pool every
// generation's scoring already goes through (see evolvers/standard.hpp's
// ScorePool). Exposed here so the initial ancestor population -- built by
// cli.cpp/gui.cpp before any Evolver is involved -- doesn't have to score
// one genome at a time on a single thread: that was fine for the
// Brainfuck-family engines but, for the neural engine, one score() call is
// expensive enough that a 10,000-genome population could take minutes
// instead of seconds serially.
void parallel_score(size_t begin, size_t total);

// Saturating a-b, floored at 1 (never 0). A couple of call sites derive a
// "how aggressive should mutation be" value from State::def_rand minus
// however many generations have been stuck (evolvers/standard.hpp,
// evolvers/above_average.cpp) -- once the stuck count catches up to
// def_rand that difference hits exactly 0, and 0 is a live SIGFPE a few
// calls downstream (rand() % 0 / rng_below(0) in the engines, or a bare
// "/ (def_rand - 50)" in above_average.cpp). Plain size_t subtraction would
// also wrap to a huge number instead of erroring if b > a, which is just as
// wrong, so this floors instead of wrapping either way.
inline size_t clamped_sub(size_t a, size_t b) { return a > b ? a - b : 1; }

class Test {
    public:
        virtual ~Test() = default;
        virtual size_t score(const void * code) const = 0;
        // Optional human-readable summary of how the given genome does
        // against this generation's trials, e.g. "7/10 correct" or "140/200
        // games won" -- a count is meaningful regardless of how many trials
        // a test runs per genome, unlike raw output text (which only ever
        // shows one trial's result). Empty string means "not shown".
        virtual std::string display(const void * code) const { return ""; }
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

    // Shared engine->run() iteration cap, replacing what used to be a
    // separate hardcoded constant per test (Output's MAX_RUNTIME, Add/Crc8's
    // LOOP_MAX, TicTacToe's MAX_TIC_TAC_TOE, Tic_Off's MAX_TIC_OFF) -- one
    // knob instead of five, since a test no longer has any way to know what
    // a "reasonable" cap is better than the user configuring it directly.
    inline size_t max_runtime = 100000;

    inline size_t repetitions = 0;
    inline size_t runs = 0;

    inline const char * output = "data.txt";
}

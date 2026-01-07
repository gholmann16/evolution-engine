#pragma once
#include <stddef.h>
#include <memory>
#include <map>

struct Program {
    void * code;
    size_t score;
};

class Engine {
    public:
        virtual ~Engine() = default;

        // input read only, output should be non freeable memory, too many allocs otherwise. Return looptime/runtime
        virtual size_t run(const void * code, char input[256], char output[256], size_t max) = 0;

        // Evolving
        virtual void * ancestor_prog() = 0;
        virtual void evolve(const void * parent, void * child, size_t randomness) = 0;

        // Utility
        virtual std::string debug(const void * code) = 0; // Should be an allocated string
        virtual std::string compile(const void * code) = 0; // Allocated string ready to be passed to assembler
        virtual bool equal(const void * first, const void * second) = 0;
        virtual size_t size(const void * code) = 0;
        virtual void copy_into(const void * parent, void * child) = 0;
};

extern Engine * engines[];
extern const char * engine_names[];
extern int num_engines;

class Test {
    public:
        virtual ~Test() = default;
        virtual size_t score(const void * code) const = 0;
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
    inline Engine * engine = nullptr;
    inline Evolver * evolver = nullptr;

    inline size_t def_rand = 250;
    inline bool verbose = false;
    inline size_t seed = 0;

    inline size_t repetitions = 0;
    inline size_t runs = 0;

    inline const char * output = "data.txt";
}

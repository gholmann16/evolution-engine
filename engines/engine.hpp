#pragma once
#include <stddef.h>
#include <stdlib.h>
#include <string>

class Engine {
    public:
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
};

Engine * create_brainfuck(char * initial);
Engine * create_jitfuck(char * initial);
Engine * create_network();

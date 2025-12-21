#pragma once
#include <stddef.h>
#include <stdlib.h>
#include <string>

#define VERBOSE true

struct Program {
    std::string code;
    size_t score;
};

class Engine {
    public:
        // input read only, output should be non freeable memory, too many allocs otherwise. Return looptime/runtime
        virtual size_t run(const std::string& code, char input[256], char output[256], size_t max) = 0;

        // Evolving
        virtual std::string ancestor_prog() = 0;
        virtual void evolve(const std::string& parent, std::string& child, size_t randomness) = 0; 

        // Utility
        virtual std::string debug(const std::string& code) = 0; // Should be an allocated string
        virtual std::string compile(const std::string& code) = 0; // Allocated string ready to be passed to assembler
        virtual bool equal(const std::string& first, const std::string& second) {
            return first == second;
        }
};

Engine * create_brainfuck(char * initial);
Engine * create_jitfuck(char * initial);
Engine * create_network();

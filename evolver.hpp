#pragma once
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#define MAX_SIZE 65533

struct Program {
    char code[MAX_SIZE]; // No 0 at the end, don't use strlen
    size_t size;
    size_t runtime;
    size_t score;
};

struct State {
    long int seed;
    int runs;
    int total_winners;
    int def_rand;
    int repetitions;
    struct Program * parent; // struct Program * total_winners
    struct Program * children; // struct Program * total_winners^2
};

class Engine {
    public:
        // input read only, output should be non freeable memory, too many allocs otherwise
        virtual void run(struct Program * prog, char input[256], char output[256], size_t max) = 0;

        // Evolving
        virtual struct Program ancestor_prog() = 0;
        virtual struct Program evolve(struct Program parent, size_t randomness, const char * allowed_chars) = 0; 

        // Utility
        virtual char * debug(struct Program prog) = 0; // Should be an allocated string
        virtual char * compile(struct Program program) = 0; // Allocated string ready to be passed to assembler
};

Engine * create_brainfuck(const char * initial);
Engine * create_jitfuck(const char * intial);


class Test {
    public:
        virtual void score(struct Program * prog, Engine * engine) = 0;
        virtual void prepare_answer() = 0;
        virtual const char * allowed_chars() = 0;
        virtual void display(struct Program * prog, Engine * engine) = 0;

    protected:
        char ** inputs = (char **)calloc(10, sizeof(char *)); // up to 10 tests
        char ** expect = (char **)calloc(10, sizeof(char *)); // up to 10 tests
        void fill_string(char input[256]) {
            for (int i = 0; i < 255; i++) {
                input[i] = rand() % 256;
            }
            input[255] = 0;
        }

        void empty(char input[256]) {
            for (int i = 0; i < 255; i++) {
                input[i] = 0;
            }
        }
        ~Test() {
            free(inputs);
            free(expect);
        }
};

// All the tests:
Test * create_crc8();
Test * create_output();
Test * create_tictactoe();

// Init
struct State def_state();

// Executing
bool generation(struct State state, Test * engine, Engine * tester);

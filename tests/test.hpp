#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <evolver.h>

class Test {
    public:
        virtual void score(struct Program * prog) = 0;
        virtual void prepare_answer() = 0;
        virtual const char * allowed_chars() = 0;
        virtual void display(struct Program * prog) = 0;

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

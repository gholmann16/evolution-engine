#pragma once
#include <stdlib.h>

class Test {
    public:
        virtual void answer(char input[256], char expect[256]) = 0;
        virtual bool read_all() = 0;
        virtual bool exact() = 0;
        virtual int reps() = 0;
        virtual unsigned long long max_runtime() = 0;
        virtual const char * allowed_chars() = 0;

    protected:
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
};

// All the tests:
Test * create_crc8();
Test * create_output();

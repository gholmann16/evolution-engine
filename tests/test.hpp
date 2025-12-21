#include <evolver.hpp>

class Test {
    public:
        virtual void score(struct Program * prog, Engine * engine) = 0;
        virtual void prepare_answer() = 0;
        virtual void display(struct Program * prog, Engine * engine) = 0;

    protected:
        char ** inputs = (char **)calloc(10, sizeof(char *)); // up to 10 tests
        char ** expect = (char **)calloc(10, sizeof(char *)); // up to 10 tests

        ~Test() {
            free(inputs);
            free(expect);
        }
};

// All the tests:
Test * create_crc8();
Test * create_output();
Test * create_tictactoe();

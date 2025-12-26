#include "../engines/engine.hpp"
#include "../state.hpp"

class Test {
    public:
        virtual void score(struct Program * prog, Engine * engine, const State * state) = 0;
};

// All the tests:
Test * create_crc8();
Test * create_output();
Test * create_tictactoe();
Test * create_tic_off();

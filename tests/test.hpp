#pragma once
#include "../engines/engine.hpp"
#include "../state.hpp"

class Test {
    public:
        virtual unsigned long long score(const void * code, Engine * engine, const State * state) = 0;
};

// All the tests:
Test * create_crc8();
Test * create_output();
Test * create_tictactoe();
Test * create_tic_off();

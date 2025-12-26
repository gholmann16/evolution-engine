#include <evolver.hpp>

class Competition {
    public:
        // false = 0 = first wins
        // true = 1 = second wins
        virtual bool fight(const void * first, const void * second, Engine * engine) = 0;
};

Competition * create_tic_off();

#include <evolver.hpp>

class Competition {
    public:
        // false = 0 = first wins
        // true = 1 = second wins
        virtual bool fight(const std::string& first, const std::string& second, Engine * engine) = 0;
};

Competition * create_tic_off();

#include <runner.hpp>

int main(int argc, char * argv[]) {
    struct State state = (argc > 1) ? load(argv[1]) : def_state();
    runner(state);
}

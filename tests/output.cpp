#include <string.h>
#include "test.hpp"

class Output : public Test {
    public:
    void answer(char input[256], char expect[256]) override {
        empty(input);
        strcpy(expect, "Hello world!");
    }

    bool read_all() override {
        return false;
    }

    bool exact() override {
        return false;
    }

    int reps() override {
        return 1;
    }

    unsigned long long max_runtime() override {
        return 100000;
    }

    const char * allowed_chars() override {
        return "+-<>[].";
    }
};

Test * create_output() {
    return new Output();
}

#include <string.h>
#include "fill.h"

void answer(char input[256], char expect[256]) {
    empty(input);
    strcpy(expect, "Hello world!");
}

bool read_all() {
    return false;
}

bool exact() {
    return false;
}

int reps() {
    return 1;
}

char * def_code() {
    return ".";
}

unsigned long long max_runtime() {
    return 500000;
}

char * allowed_chars() {
    return "+-<>[].";
}

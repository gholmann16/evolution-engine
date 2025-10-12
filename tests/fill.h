#include <stdlib.h>

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
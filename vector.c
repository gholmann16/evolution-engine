#include <stdlib.h>

struct Vector init_vec() {
    struct ret = {
        .data = malloc(256);
        .size = 0;
        .capacity = 256;
    }
    return ret;
}

void push(struct Vector vec, unsigned int add) {
    if (size == capacity) {
        vec.data = realloc(vec.data, capacity*2);
        vec.capacity = capacity*2;
    }
    vec.data[vec.size] = add;
}

unsigned int pop(struct Vector vec) {
    return vec[--vec.size];
}

void delete_vec(struct Vector vec) {
    free(vec.data);
}

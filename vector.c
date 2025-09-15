#include <stdlib.h>
#include "vector.h"

struct Vector init_vec() {
    return (struct Vector){.data = malloc(256), .capacity = 256};
}

void push(struct Vector * vec, unsigned int add) {
    if (vec->size == vec->capacity) {
        vec->data = realloc(vec->data, vec->capacity*2);
        vec->capacity = vec->capacity*2;
    }
    vec->data[vec->size++] = add;
}

unsigned int pop(struct Vector * vec) {
    return vec->data[--vec->size];
}

void clear_vec(struct Vector * vec) {
    vec->size = 0;
}

void delete_vec(struct Vector vec) {
    free(vec.data);
}

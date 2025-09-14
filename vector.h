struct Vector {
    unsigned int * data;
    size_t size;
    size_t capacity;
}

struct Vector init_vec();
void push(struct Vector vec, unsigned int add);
unsigned int pop(struct Vector vec);
void delete_vec(struct Vector vec);

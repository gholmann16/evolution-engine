#include <stddef.h>
#include <string.h>
#include "fill.h"

// Needed for little endian
#define SWAP(x) ((x>>8) & 0xff) | ((x & 0xff)<<8)

// Polynomial x^8 + x^7 + x^6 + x^4 + x^2 + x^0 shifted
short polynomials[8] = {
    SWAP(0x01D5 << 0), SWAP(0x01D5 << 1), SWAP(0x01D5 << 2), SWAP(0x01D5 << 3),
    SWAP(0x01D5 << 4), SWAP(0x01D5 << 5), SWAP(0x01D5 << 6), SWAP(0x01D5 << 7),
};

// DVB-S2
char crc8(char input[256]) {
    size_t size = strlen(input);
    for (int byte = 0; byte < size; byte++) {
        // Obtain a short pointer for word work
        short * pointer = (short *)(input + byte);
        for (int bit = 7; bit >= 0; bit--) {
            // If leading bit is 1
            if (input[byte] >> bit)
                pointer[0] ^= polynomials[bit];
        }
    }
    // Should already be allocated as originally null byte
    return input[size];
}

void answer(char input[256], char expect[256]) {
    fill_string(input);
    char tmp[256];
    memcpy(tmp, input, 256);
    expect[0] = crc8(tmp);
    expect[1] = 0;
}

bool read_all() {
    return false;
}

bool exact() {
    return true;
}

int reps() {
    return 3;
}

unsigned long long max_runtime() {
    return 2000000;
}

char * allowed_chars() {
    return "+-<>[].,";
}

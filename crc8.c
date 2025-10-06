#include <stdio.h>
#include <stddef.h>
#include <string.h>

// Needed for little endian
#define SWAP(x) ((x>>8) & 0xff) | ((x & 0xff)<<8)

// Polynomial x^8 + x^7 + x^6 + x^4 + x^2 + x^0 shifted
short polynomials[8] = {
    SWAP(0x01D5 << 0), SWAP(0x01D5 << 1), SWAP(0x01D5 << 2), SWAP(0x01D5 << 3),
    SWAP(0x01D5 << 4), SWAP(0x01D5 << 5), SWAP(0x01D5 << 6), SWAP(0x01D5 << 7),
};

// DVB-S2
char crc8(char * input) {
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

#include <stdlib.h>
#include "suncrisc.h"

struct String {
    char * contents;
    size_t capacity;
}

void append(struct String nasm, char * addition) {
    if (strlen(nasm.contents) + strlen(addition) + 1 > capacity) // Must have 1 more for ending \0, so >=
        nasm.contents = realloc(nasm.contents, strlen(nasm.contents) + strlen(addition) + 1);

    strcat(nasm.contents, addition);
}

struct String init() {
    struct String ret = {
        .contents = malloc(256);
        .capacity = 256;
    }
    return ret;
}

char * begin_boiler = 
    "global _start\n"
    "section .bss\n" // Declare memory in .bss space
    "memory: resb 65536\n" // Automatically set to 0 because in bss
    "section .text\n"
    "_start:\n"
    "xor rax, rax\nxor rbx, rbx\nxor rcx, rcx\nxor rdx, rdx\n"
    "xor rsi, rsi\nxor rdi, rdi\nxor rbp, rbp\nxor rsp, rsp\n"
    "xor r8 , r8 \nxor r9 , r9 \nxor r10, r10\nxor r11, r11\n"
    "xor r12, r12\nxor r13, r13\nxor r14, r14\nxor r15, r15\n"
    ;

char * translate(short opcode) {
    char * translation = malloc(256);
    char * registers = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"};
        int regnum = opcode & TO_REG_MASK;
    switch (opcode & OPCODE_MASK) {
        case 0b00:
            strcpy(translation, "add ");
            
    }
}

char * compile(struct Program program) {
    struct String nasm = init();

}
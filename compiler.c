#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sucrisc.h"

struct String {
    char * contents;
    size_t capacity;
};

void append(struct String * nasm, char * addition) {
    if (strlen(nasm->contents) + strlen(addition) + 1 > nasm->capacity) // Must have 1 more for ending \0, so >=
        nasm->contents = realloc(nasm->contents, (strlen(nasm->contents) + strlen(addition))*2);

    strcat(nasm->contents, addition);
}

struct String init() {
    return (struct String){.contents = calloc(256, sizeof(char)), 256};
}

char * registers[] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"};

void fill(char ** dirop, short opcode) {
    char * translation = *dirop;
    int toreg = (opcode & TO_REG_MASK) >> TO_REG_SHIFT;
    int fromreg = opcode & FROMREGMASK;

    if (opcode & TO_MEM_MASK)
        strcat(translation, "byte[memory + ");
    strcat(translation, registers[toreg]);
    if (opcode & TO_MEM_MASK)
        strcat(translation, "]");
    strcat(translation, ", ");

    char val[32];
    // int offset = ((opcode & OFFSET_MASK) >> OFFSET_SHIFT) - 4;

    if (opcode & OR_NUM_MASK)
        sprintf(val, "%d", opcode & DIRNUM_MASK);
    else if (opcode & FROMMEMMASK)
        sprintf(val, "byte[memory + %s]", registers[fromreg]);
    else // raw reg
        sprintf(val, registers[fromreg]);

    strcat(translation, val);
    strcat(translation, "\n");
}

char * translate(char ** write, short opcode, size_t * count) {
    char * translation = *write;

    switch (opcode & OPCODE_MASK) {
        case ADD:
            strcpy(translation, "add ");
            fill(write, opcode);
            break;
        case MOV:
            strcpy(translation, "mov ");
            fill(write, opcode);
            break;
        case PT :
            sprintf(translation, "point_%d:\n", *count--);
            break;
        case JE :
            sprintf(translation, "cmp ");
            fill(write, opcode);
            char tmp[32];
            sprintf(tmp, "je point_%d\n");
            strcat(translation, tmp);
            break;
    }

    return translation;
}

char * compile_core(struct Program program) {
    struct String nasm = init();
    char * line_data = malloc(256);

    size_t count = 0;
    for (size_t line = 0; line < program.size; line++)
        append(&nasm, translate(&line_data, program.code[line], &count));

    free(line_data);
    return nasm.contents;
}

char * begin_boiler = 
    "global _start\n"
    "section .bss\n" // Declare memory in .bss space
    "memory: resb 65536\n" // Automatically set to 0 because in bss
    "section .text\n"
    "_start:\n"
    "mov rax, 0\n" // Read user input to memory
    "mov rdi, 0\n"
    "mov rsi, memory\n"
    "mov rdx, 65536\n"
    "syscall\n"
    "cmp rax, -1\n"
    "je fail\n"
    "xor rax, rax\nxor rbx, rbx\nxor rcx, rcx\nxor rdx, rdx\n"
    "xor rsi, rsi\nxor rdi, rdi\nxor rbp, rbp\nxor rsp, rsp\n"
    "xor r8, r8\nxor r9, r9\nxor r10, r10\nxor r11, r11\n"
    "xor r12, r12\nxor r13, r13\nxor r14, r14\nxor r15, r15\n"
    ;

char * end_boiler =
    "mov rax, 1\n" // Write result to terminal
    "mov rdi, 1\n"
    "mov rsi, memory\n"
    "add rsi, rbx\n" // Get data from register essentially
    "mov rcx, rsi\n"
    "mov rdx, 0\n" // Count the bytes in memory
    "continue:\n"
    "cmp byte[rcx], 0\n"
    "je done\n"
    "add rcx, 1\n" // Move index forward
    "add rdx, 1\n" // Add to count
    "jmp continue\n"
    "done:\n"
    "syscall\n"
    "cmp rax, -1\n"
    "je fail\n"
    "mov rdi, 0\n"
    "mov rax, 60\n"
    "syscall\n"
    "fail:\n"
    "mov rdi, 1\n"
    "mov rax, 60\n"
    "syscall\n"
    ;

char * compile(struct Program program) {
    struct String nasm = init();
    char * code = compile_core(program);

    append(&nasm, begin_boiler);
    append(&nasm, code);
    append(&nasm, end_boiler);

    free(code);
    return (nasm.contents);
}
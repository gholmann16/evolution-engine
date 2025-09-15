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

#define QWORD 0
#define WORD 1
#define BYTE 2

char * registers[][16] = {
    {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"},
    {"ax", "bx", "cx", "dx", "si", "di", "bp", "sp", "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"},
    {"al", "bl", "cl", "dl", "sil", "dil", "bpl", "spl", "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"},
};

void fill(char ** dirop, short opcode, char * operation) {
    char * translation = *dirop;
    int toreg = (opcode & TO_REG_MASK) >> TO_REG_SHIFT;
    if (opcode & OR_NUM_MASK)
        sprintf(translation, (opcode & TO_MEM_MASK) ? "%s byte[memory + %s], %d\n" : "%s %s, %d\n", 
            operation, registers[(opcode & TO_MEM_MASK) ? QWORD : WORD][toreg], opcode & DIRNUM_MASK);

    int fromreg = opcode & FROMREGMASK;

    switch (opcode & MEMREG_MASK) {
        case MEM_TO_MEM:
            sprintf(translation, "%s byte[memory + %s], byte[memory + %s]\n", 
                operation, registers[QWORD][toreg], registers[QWORD][fromreg]);
            break;
        case MEM_TO_REG:
            sprintf(translation, "mov byte[conversion], 0\nmov byte[conversion+1], byte[memory + %s]\n%s %s, word[conversion]\n", 
                registers[QWORD][fromreg], operation, registers[WORD][toreg]);
            break;
        case REG_TO_MEM:
            sprintf(translation, "%s byte[memory + %s], %s\n",
                operation, registers[QWORD][toreg], registers[BYTE][fromreg]);
            break;
        case REG_TO_REG:
            sprintf(translation, "%s %s, %s\n", operation, registers[WORD][toreg], registers[WORD][fromreg]);
    }
}

char * translate(char ** write, short opcode, size_t * count) {
    char * translation = *write;

    switch (opcode & OPCODE_MASK) {
        case ADD:
            fill(write, opcode, "add");
            break;
        case MOV:
            fill(write, opcode, "mov");
            break;
        case PT :
            sprintf(translation, "point_%d:\n", *count--);
            break;
        case JE :
            fill(write, opcode, "jmp");
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
    "conversion: resb 2\n"
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
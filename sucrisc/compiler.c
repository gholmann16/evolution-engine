#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../main.h"
#include "vector.h"
#include "sucrisc.h"

struct String {
    char * contents;
    size_t capacity;
};

bool append(struct String * nasm, char * addition) {
    if (addition == NULL)
        return false;
    if (strlen(nasm->contents) + strlen(addition) + 1 > nasm->capacity) // Must have 1 more for ending \0, so >=
        nasm->contents = realloc(nasm->contents, (strlen(nasm->contents) + strlen(addition))*2);

    strcat(nasm->contents, addition);
    return true;
}

struct String init() {
    return (struct String){.contents = calloc(256, sizeof(char)), 256};
}

#define QWORD 0
#define WORD 1
#define BYTE 2

char * registers[][16] = { // Registers rax, rdx, rdi, rsi, r8, r9, r10 used in syscalls, r11 used as tmp
    {"rbx", "rcx", "rbp", "rsp", "r12", "r13", "r14", "r15"},
    {"bx", "cx", "bp", "sp", "r12w", "r13w", "r14w", "r15w"},
    {"bl", "cl", "bpl", "spl", "r12b", "r13b", "r14b", "r15b"},
};

void fill(char ** dirop, union Operation op, char * operation) {
    char * translation = *dirop;
    if (op.or_num) {
        sprintf(translation, (op.opcode > 3) ? "%s byte[memory + %s], %d\n" : "%s %s, %d\n", 
            operation, registers[(op.opcode > 3) ? QWORD : WORD][op.to_reg], op.number);
        return;
    }
    
    switch (op.fr_mem*2 + (op.opcode > 3)) {
        case 0b11: // Memory to memory is not supported in x86
            sprintf(translation, "mov r11b, byte[memory + %s]\n%s byte[memory + %s], r11b\n",
                registers[QWORD][op.fr_reg], operation, registers[QWORD][op.to_reg]);
            break;
        case 0b10: // Can't add one byte to a register without zero extending
            sprintf(translation, "movzx r11w, byte[memory + %s]\n%s %s, r11b\n", 
                registers[QWORD][op.fr_reg], operation, registers[WORD][op.to_reg]);
            break;
        case 0b01:
            sprintf(translation, "%s byte[memory + %s], %s\n",
                operation, registers[QWORD][op.to_reg], registers[BYTE][op.fr_reg]);
            break;
        case 0b00:
            sprintf(translation, "%s %s, %s\n", operation, registers[WORD][op.to_reg], registers[WORD][op.fr_reg]);
            break;
    }
}

char * translate(char ** write, union Operation op, unsigned int line, struct Vector * points) {
    char * translation = *write;
    char tmp[32];

    switch (op.opcode) {
        case ADD:
        case ADDM:
            fill(write, op, "add");
            break;
        case MOV:
        case MOVM:
            fill(write, op, "mov");
            break;
        case PT:
            push(points, line);
            sprintf(tmp, "point_%d:\n", line);
            strcpy(translation, tmp);
            break;
        case NOP:
            strcpy(translation, "nop");
            break;
        case JE:
        case JEM:
            fill(write, op, "jmp");
            unsigned int last = pop(points);
            if (last == -1)
                return NULL;
            sprintf(tmp, "je point_%d\n", last);
            strcpy(translation, tmp);
            break;
    }

    return translation;
}

char * debug(struct Program program) {
    struct String nasm = init();
    char * line_data = malloc(256);

    static struct Vector points = {0};
    if (points.capacity == 0)
        points = init_vec();
    clear_vec(&points);

    for (unsigned int line = 0; line < (program.size / sizeof(union Operation)); line++)
        if(append(&nasm, translate(&line_data, ((union Operation *)program.code)[line], line, &points)) == false) {
            sprintf(nasm.contents, "Error with operation %d, too many jumps for amount of points.\n", line);
            break;
        }

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
    "xor rbx, rbx\n"
    "xor rcx, rcx\n"
    "xor rbp, rbp\n"
    "xor rsp, rsp\n"
    "xor r12, r12\n"
    "xor r13, r13\n"
    "xor r14, r14\n"
    "xor r15, r15\n"
    ;

char * end_boiler =
    "mov rax, 1\n" // Write result to terminal
    "mov rdi, 1\n"
    "mov rsi, memory\n"
    "add rsi, rbx\n" // Get data from register 0 essentially
    "mov r11, rsi\n"
    "mov rdx, 0\n" // Count the bytes in memory
    "continue:\n"
    "cmp byte[r11], 0\n"
    "je done\n"
    "add r11, 1\n" // Move index forward
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
    char * code = debug(program);

    append(&nasm, begin_boiler);
    append(&nasm, code);
    append(&nasm, end_boiler);

    free(code);
    return (nasm.contents);
}

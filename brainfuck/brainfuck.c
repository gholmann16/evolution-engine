#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <main.h>

#define MAX_RUNTIME 100000
#define MAX_OUTPUT 1024

struct Program ancestor_prog() {
    return (struct Program){.code = ".[>.]", .size = 5};
}

char * debug(struct Program prog) {
    return strdup(prog.code);
}

bool validate(struct Program prog) {
    int open = 0;
    for (int x = 0; x < prog.size; x++) {
        switch (((char *)prog.code)[x]) {
            case '[':
                open++;
                break;
            case ']':
                open--;
                break;
        }
        if (open < 0)
            return true; // Mismatching close error
    }
    if (open != 0) // Mismatching bracket error
        return true;

    return false;
}

char * run(struct Program prog, unsigned char memory[65536]) {
    if(validate(prog) == true)
        return NULL; //Punishment for failing

    char output[MAX_OUTPUT];
    char * code = prog.code;

    unsigned int index = 0;
    unsigned short location = 0;
    unsigned int runtime = 0;

    for (int x = 0; x < prog.size; x++) {
        if (++runtime == MAX_RUNTIME)
            return NULL;

        switch(code[x]) {
            case '+': 
                memory[location]++;
                break;
            case '-':
                memory[location]--;
                break;
            case '>':
                location++;
                break;
            case '<':
                location--;
                break;
            case '[':
                if (memory[location] == 0) {
                    int brackets = 1;
                    while (brackets) {
                        x++;
                        if (code[x] == '[')
                            brackets++;
                        else if (code[x] == ']')
                            brackets--;
                    }
                }
                break;
            case ']':
                if (memory[location]) {
                    int rev = 1;
                    while(rev) {
                        x--;
                        if (code[x] == ']')
                            rev++;
                        else if (code[x] == '[')
                            rev--;
                    }
                }
                break;
            case '.':
                if (index == MAX_OUTPUT)
                    return NULL;
                output[index] = memory[location];
                index++;
                break;
            default:
                puts("Unknown brainfuck command detected");
                printf("chracter hex: 0x%x\n", code[x]);
                puts("Code:");
                puts(code);
                exit(-1);
        }
    }

    strcpy((char *)memory, output);
    return (char *)memory;
}

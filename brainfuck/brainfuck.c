#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <evolver.h>

void init_env() {
    ;
}
void free_env() {
    ;
}

struct Program ancestor_prog() {
    char * code = "+";
    struct Program def = {0};
    strcpy(def.code, code);
    def.size = strlen(code);
    return def;
}

char * debug(struct Program prog) {
    return strdup(prog.code);
}

bool validate(struct Program prog) {
    int open = 0;
    for (int x = 0; x < prog.size; x++) {
        switch (prog.code[x]) {
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

void run(struct Program * prog, char input[256], char output[256]) {
    if(validate(*prog) == true) {
        prog->runtime = max_runtime();
        return; // Punishment for failing
    }

    unsigned char memory[65536] = {0};
    unsigned short jump_points[MAX_SIZE / 2]; // Even if odd, can't have odd brackets
    unsigned short jump_pointer = 0;

    unsigned char out_dex = 0;
    unsigned char in_dex = 0;
    unsigned short location = 0;

    for (int x = 0; x < prog->size; x++) {
        switch(prog->code[x]) {
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
                jump_points[jump_pointer++] = x;
                int brackets = 1;
                while (brackets) {
                    x++;
                    if (prog->code[x] == '[')
                        brackets++;
                    else if (prog->code[x] == ']')
                        brackets--;
                }
                // Could do x-- and break, or just continue
            case ']':
                if (++prog->runtime == max_runtime())
                    return;
                if (memory[location])
                    x = jump_points[jump_pointer - 1];
                else
                    jump_pointer--;
                break;
            case '.':
                output[out_dex++] = memory[location];
                break;
            case ',':
                memory[location] = input[in_dex++];
                break;
            case '0':
                memory[location] = 0;
                break;
            default:
                puts("Unknown brainfuck command detected");
                printf("chracter hex: 0x%x\n", prog->code[x]);
                puts("Code:");
                puts(prog->code);
                exit(-1);
        }
    }

    if (!exact())
        output[out_dex] = 0;
}

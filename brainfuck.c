#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#define MAX_RUNTIME 100000
#define MAX_OUTPUT 1024

int validate(char * code) {
    int open = 0;
    for (int x = 0; x < strlen(code); x++) {
        switch (code[x]) {
            case '[':
                open++;
                break;
            case ']':
                open--;
                break;
        }
        if (open < 0)
            return 1; // Mismatching close error
    }
    if (open != 0) // Mismatching bracket error
        return 2;

    return 0;
}

char * run(char * code, char * mem, unsigned short size) {

    if(validate(code) != 0)
        return NULL; //Punishment for failing

    char memory[65536] = {0};
    char * output = calloc(MAX_OUTPUT, sizeof(char));
    unsigned int index = 0;
    unsigned short location = 0;
    unsigned int runtime = 0;

    for (int a = 0; a < size; a++) {
        memory[a] = mem[a];
    }

    for (int x = 0; x < strlen(code); x++) {
        if (++runtime == MAX_RUNTIME) {
            free(output);
            return NULL;
        }

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
                if (index == MAX_OUTPUT) {
                    free(output);
                    return NULL;
                }
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

    return output;
}

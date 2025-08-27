#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#define MAX_RUNTIME 100000

int run(char * code, char * mem, unsigned short size) {

    unsigned char memory[65536] = {0};
    unsigned short location = 0;
    unsigned int runtime = 0;

    for (int a = 0; a < size; a++) {
        memory[a] = mem[a];
    }

    for (int x = 0; x < strlen(code) && runtime < MAX_RUNTIME; x++) {
        runtime++;
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
                        if (x >= strlen(code))
                            return -1;
                        if (code[x] == '[')
                            brackets++;
                        else if (code[x] == ']')
                            ; // Don't allow code to run because confirmed it skips
                    }
                }
                break;
            case ']':
                if (memory[location]) {
                    int rev = 1;
                    while(rev) {
                        x--;
                        if (x < 0)
                            return -1;
                        if (code[x] == ']') {
                            rev++;
                        }
                        else if (code[x] == '[') {
                            ; // Don't allow code to run because confirmed it enters
                        }
                    }
                }
                break;
            default:
                puts("Unknown brainfuck command deteceted");
                printf("chracter hex: 0x%x\n", code[x]);
                puts("Code:");
                puts(code);
                exit(-1);
        }
    }

    return memory[location];
}

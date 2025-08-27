#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#define MAX_RUNTIME 100000

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

int run(char * code, char * mem, unsigned short size) {

    if(validate(code) != 0)
        return 9999999; //Punishment for failing

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

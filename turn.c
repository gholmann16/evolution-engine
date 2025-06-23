#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

int run(char * contents, char * mem) {

    unsigned char memory[65536] = {0};
    unsigned short location = 0;

    for (int a = 0; a < 9; a++) {
        memory[a] = mem[a];
    }

    for (int x = 0; x < strlen(contents); x++) {
        switch(contents[x]) {
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
                        if (x >= strlen(contents))
                            return -1;
                        if (contents[x] == '[')
                            brackets++;
                        else if (contents[x] == ']')
                            brackets--;
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
                        if (contents[x] == ']') {
                            rev++;
                        }
                        else if (contents[x] == '[') {
                            rev--;
                        }
                    }
                }
                break;
            default:
                puts("Unknown brainfuck command detecetd");
                puts("This should NEVER happen");
                printf("chracter hex: 0x%x\n", contents[x]);
                exit(-1);
        }
    }

    return memory[location];
}

int bf_turn(char * code, char board[9]) {
    return run(code, board);
}

int random_turn() {
    return rand() % 9;
}
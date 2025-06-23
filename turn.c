#include <stdlib.h>
#include <time.h>
#include <string.h>

int run(char * contents, char * mem) {

    unsigned char memory[65536] = {0};
    unsigned short location = 0;

    for (int a = 0; a < strlen(mem); a++) {
        memory[a] = mem[a];
    }

    for (int x = 0; x <= strlen(contents); x++) {
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
                        if (x > strlen(contents))
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
        }
    }

    return memory[location];
}

int bf_turn(char * code, char board[10]) {
    char mem [11] = {0};
    strncpy(mem, board, 10);
    return run(code, mem);
}

int random_turn() {
    return rand() % 9;
}
#include <stdlib.h>
#include <time.h>
#include <globals.h>

int run(char * contents, int len, char * mem) {

    unsigned char memory[65536] = {0};
    unsigned short location = 0;

    for (int a = 0; a < strlen(mem); a++) {
        memory[a] = mem[a];
    }

    for (int x = 0; x <= len; x++) {
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
                        if (x > len)
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

int turn1(char mark, char board[10]) {
    char mem [11] = {0}
    strncpy(mem, board, 10);
    return run(program_contents, program_len, mem) % 10;
}

int turn2() {
    
}
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include "brainfuck_base.hpp"

class Brainfuck : public Brainfuck_Base {
    private:
        bool validate(struct Program prog) {
            int open = 0;
            for (size_t x = 0; x < prog.size; x++) {
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
    public:
        Brainfuck(const char * initial) : Brainfuck_Base(initial) {
            ;
        }

        void run(struct Program * prog, char input[256], char output[256], size_t max) {
            if(validate(*prog) == true) {
                prog->runtime = max;
                return; // Punishment for failing
            }

            unsigned char memory[65536] = {0};
            unsigned short jump_points[MAX_SIZE / 2]; // Even if odd, can't have odd brackets
            unsigned short jump_pointer = 0;

            unsigned char out_dex = 0;
            unsigned char in_dex = 0;
            unsigned short location = 0;
            int brackets;

            for (size_t x = 0; x < prog->size; x++) {
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
                        brackets = 1;
                        while (brackets) {
                            x++;
                            if (prog->code[x] == '[')
                                brackets++;
                            else if (prog->code[x] == ']')
                                brackets--;
                        }
                        // Could do x-- and break, or just continue
                    case ']':
                        if (++prog->runtime == max)
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
        }
};

Engine * create_brainfuck(const char * initial) {
    return new Brainfuck(initial);
}

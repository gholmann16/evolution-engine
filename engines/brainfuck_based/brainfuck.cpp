#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include "brainfuck_base.hpp"

class Brainfuck : public Brainfuck_Base {
    private:
        bool validate(const std::string& code) {
            int open = 0;
            for (size_t x = 0; x < code.size(); x++) {
                switch (code[x]) {
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
        size_t run(const void * code, char input[256], char output[256], size_t max) {
            const std::string& code_ref = *reinterpret_cast<const std::string*>(code);
            if(validate(code_ref) == true)
                return max; // Punishment for failing

            unsigned char memory[65536] = {0};
            unsigned short jump_points[100000]; // hopefully not more than 100000 jumps
            unsigned short jump_pointer = 0;

            unsigned char out_dex = 0;
            unsigned char in_dex = 0;
            unsigned short location = 0;
            size_t runtime = 0;
            int brackets;

            for (size_t x = 0; x < code_ref.size(); x++) {
                switch(code_ref[x]) {
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
                            if (code_ref[x] == '[')
                                brackets++;
                            else if (code_ref[x] == ']')
                                brackets--;
                        }
                        // Could do x-- and break, or just continue
                    case ']':
                        if (++runtime == max)
                            return max;
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
                        printf("chracter hex: 0x%x\n", code_ref[x]);
                        puts("Code:");
                        std::cout << code_ref << std::endl;
                        exit(-1);
                }
            }
            return max;
        }
};

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

        const std::string * cp;
        bool invalid;
    public:
        void load(const void * code) {
            cp = reinterpret_cast<const std::string*>(code);
            invalid = validate(*cp);
        }
        size_t run(char input[256], char output[256], size_t max) {
            if(invalid == true)
                return max; // Punishment for failing

            unsigned char memory[65536] = {0};
            unsigned short jump_points[100000]; // hopefully not more than 100000 jumps
            unsigned short jump_pointer = 0;

            unsigned char out_dex = 0;
            unsigned char in_dex = 0;
            unsigned short location = 0;
            size_t runtime = 0;
            int brackets;

            for (size_t x = 0; x < cp->size(); x++) {
                switch((*cp)[x]) {
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
                            if ((*cp)[x] == '[')
                                brackets++;
                            else if ((*cp)[x] == ']')
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
                        printf("chracter hex: 0x%x\n", (*cp)[x]);
                        puts("Code:");
                        std::cout << (*cp) << std::endl;
                        exit(-1);
                }
            }
            return max;
        }
};

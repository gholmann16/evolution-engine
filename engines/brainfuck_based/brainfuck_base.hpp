#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <evolver.hpp>

class Brainfuck_Base : public Engine {
    private:
        std::string initial;

    public:
        Brainfuck_Base(char * initial) {
            this->initial = (initial == NULL) ? std::string("+") : std::string(initial);
        }

        std::string ancestor_prog() {
            return initial;
        }

        void evolve(const std::string& parent, std::string& child, size_t randomness) {
            const char * allowed_chars = "+-<>[].,";
            int len = strlen(allowed_chars);
            child.clear();

            for (size_t gene = 0; gene < parent.size(); gene++) {
                int decision = rand() % randomness;
                switch (decision) {
                    case 0: // 1 % chance you remove code
                        break;
                    case 1: // 1 % chance you add code
                        gene--;
                        child += allowed_chars[rand() % len];
                        break;
                    case 2:
                    case 3: // 3% chance you modify
                    case 4:
                        child += allowed_chars[rand() % len];
                        break;
                    default: // 95 % chance you do nothing (slightly more because additions go here after)
                        child += parent[gene];
                        break;
                }
            }
        }

        std::string debug(const std::string& code) {
            return code;
        }

        std::string compile(const std::string& code) {
            return NULL;
        }
};

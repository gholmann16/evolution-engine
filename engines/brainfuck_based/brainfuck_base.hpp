#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include "../engine.hpp"

class Brainfuck_Base : public Engine {
    private:
        std::string initial;

    public:
        Brainfuck_Base(char * initial) {
            this->initial = (initial == NULL) ? std::string("+") : std::string(initial);
        }

        void * ancestor_prog() {
            return new std::string(initial);
        }

        void evolve(const void * parent, void * child, size_t randomness) {
            const std::string& parent_ref = *reinterpret_cast<const std::string*>(parent);
            std::string& child_ref = *reinterpret_cast<std::string*>(child);

            const char * allowed_chars = "+-<>[].,";
            int len = strlen(allowed_chars);
            child_ref.clear();

            for (size_t gene = 0; gene < parent_ref.size(); gene++) {
                int decision = rand() % randomness;
                switch (decision) {
                    case 0: // 1 % chance you remove code
                        break;
                    case 1: // 1 % chance you add code
                        gene--;
                        child_ref += allowed_chars[rand() % len];
                        break;
                    case 2:
                    case 3: // 3% chance you modify
                    case 4:
                        child_ref += allowed_chars[rand() % len];
                        break;
                    default: // 95 % chance you do nothing (slightly more because additions go here after)
                        child_ref += parent_ref[gene];
                        break;
                }
            }
        }

        std::string debug(const void * code) {
            const std::string& code_ref = *reinterpret_cast<const std::string*>(code);
            return code_ref; // copies automatically
        }

        std::string compile(const void * code) {
            return NULL;
        }

        bool equal(const void * first, const void * second) {
            const std::string& first_ref = *reinterpret_cast<const std::string*>(first);
            const std::string& second_ref = *reinterpret_cast<const std::string*>(second);
            return first_ref == second_ref;
        }

        size_t size(const void * code) {
            const std::string& code_ref = *reinterpret_cast<const std::string*>(code);
            return code_ref.size();
        }

        void copy_into(const void * parent, void * child) {
            const std::string& parent_ref = *reinterpret_cast<const std::string*>(parent);
            std::string& child_ref = *reinterpret_cast<std::string*>(child);
            child_ref = parent_ref;
        }
};

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <evolver.hpp>

class Brainfuck_Base : public Engine {
    public:
        Brainfuck_Base(const char * initial) {
            this->initial = (initial == NULL) ? "+" : initial;
        }

        struct Program ancestor_prog() {
            struct Program def = {0};
            strcpy(def.code, initial);
            def.size = strlen(initial);
            return def;
        }

        struct Program evolve(struct Program prog, size_t randomness, const char * chars) {
            struct Program child;
            child.size = 0;
            child.runtime = 0;
            child.score = 0;

            int max_additions = MAX_SIZE - prog.size;
            int added = 0;
            int len = strlen(chars);

            for (size_t gene = 0; gene < prog.size; gene++) {
                int decision = rand() % randomness;
                switch (decision) {
                    case 0: // 1 % chance you remove code
                        added--;
                        break;
                    case 1: // 1 % chance you add code
                        if (added < max_additions) {
                            added++;
                            gene--;
                            child.code[child.size++] = chars[rand() % len];
                        }
                        break;
                    case 2:
                    case 3: // 3% chance you modify
                    case 4:
                        child.code[child.size++] = chars[rand() % len];
                        break;
                    default: // 95 % chance you do nothing (slightly more because additions go here after)
                        child.code[child.size++] = prog.code[gene];
                        break;
                }
            }

            return child;
        }

        char * debug(struct Program prog) {
            char * ret = (char *) malloc(prog.size + 1);
            memcpy(ret, prog.code, prog.size);
            ret[prog.size] = 0;
            return ret;
        }

        char * compile(struct Program prog) {
            return NULL;
        }

    private:
        const char * initial;
};

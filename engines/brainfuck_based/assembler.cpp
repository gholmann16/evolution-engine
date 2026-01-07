#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include "brainfuck_base.hpp"

// 1. rdi = pointer to memory space (aligned so as di will overflow back to the start
// 2. rsi = initial counter, should be 0 unless you want to start with some already
// 2. rdx = pointer to input (aligned so sil will overflow back to start)
// 3. rcx = pointer to output (aligned so dl will overflow back to start)

// RDI is the memory pointer
unsigned char rig[] = {0x66, 0xFF, 0xC7}; // inc di
unsigned char lef[] = {0x66, 0xFF, 0xCF}; // dec di
unsigned char inc[] = {0xFE, 0x07}; // inc byte ptr[rdi]
unsigned char dec[] = {0xFE, 0x0F}; // dec byte ptr[rdi]
unsigned char com[] = {
    0x8A, 0x02, // mov al, byte ptr[rdx] (move input into temporary register)
    0x88, 0x07, // mov byte ptr[rdi], al
    0xFE, 0xC2, // inc dl
};
unsigned char per[] = {
    0x8A, 0x07, // mov al, byte ptr[rdi] (move current to temp)
    0x88, 0x01, // mov byte ptr[rcx], al (move temp to output)
    0xFE, 0xC1, // inc cl
};
unsigned char beg[] = {
    0xE9, 0x00, 0x00, 0x00, 0x00, // jmp + offset, have to go fill the offset.
};
unsigned char end[] = {
    0x48, 0xff, 0xce, // dec rsi
    0x48, 0x85, 0xF6, // test rsi, rsi
    0x75, 0x01, // jnz SHORT 1
    0xC3, // ret
    0xF6, 0x07, 0xFF, // test byte ptr [rdi], 0xFF
    0x0F, 0x85, 0x00, 0x00, 0x00, 0x00, // jnz + offset, have to fill the offset
};

class JitFuck : public Brainfuck_Base {
    private:
        void append(char * str, int * str_size, unsigned char * app, int app_size) {
            for(int i = 0; i < app_size; i++)
                str[(*str_size)++] = app[i];
        }

        char * program;
        const char * last = NULL;

    public:
        JitFuck() {
            program = (char*) mmap(NULL, // address
                4096 * 256, // page sizes are 4096, alloacte 256 so we always have space
                PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_ANONYMOUS,
                -1, // fd (not used here)
                0 // offset (not used here)
            );
            if (program == MAP_FAILED) {
                perror("failed to allocate memory");
                exit(1);
            }
        }

        ~JitFuck() {
            munmap(program, 4096 * 256);
        }

        size_t run(const void * code, char input[256], char output[256], size_t max) {
            const std::string& code_ref = *reinterpret_cast<const std::string*>(code);
            // clock_t begin = clock();
            int index = 0;
            int brackets = 0;

            // if (code.data() == last)
            //     goto execute;
            // else
            //     last = code.data();

            /*
            !!
            Way to make it faster: use char width enum type and then index array instead of switch case excep for ], maybe that's worth 0
            https://stackoverflow.com/questions/4879286/specifying-size-of-enum-type-in-c
            tried doing a 256 long array and directly accessing it and it wasn't any faster
            */
            int vec[65536];
            for (unsigned short x = 0; x < code_ref.size(); x++) {
                switch(code_ref[x]) {
                    case '>':
                        append(program, &index, rig, sizeof(rig));
                        break;
                    case '<':
                        append(program, &index, lef, sizeof(lef));
                        break;
                    case '+':
                        append(program, &index, inc, sizeof(inc));
                        break;
                    case '-':
                        append(program, &index, dec, sizeof(dec));
                        break;
                    case ',':
                        append(program, &index, com, sizeof(com));
                        break;
                    case '.':
                        append(program, &index, per, sizeof(per));
                        break;
                    case '[':
                        append(program, &index, beg, sizeof(beg));
                        vec[brackets++] = index;
                        break;
                    case ']':
                        if (brackets == 0)
                            return max;

                        append(program, &index, end, sizeof(end));
                        int offset = vec[--brackets] - index;
                        *(int *)(program + index - 4) = offset; // 4 less than the index after adding end
                        int skip = -offset - sizeof(end); // for the forward jump, don't skip this part
                        *(int *)(program + index + offset - 4) = skip;
                        break;
                }
            }

            program[index++] = 0xC3; // this is ret

            if (brackets != 0)
                return max;

            // execute:
            alignas(65536) unsigned char memory[65536] = {0};
            // clock_t end = clock();
            // printf("assemble time = %lf\n", (double)(end - begin) / CLOCKS_PER_SEC);
            // begin = clock();

            size_t difference;
            asm volatile(
                "call *%[fn]"
                : "=S" (difference) // rsi -> difference
                : [fn] "r" (program),
                    "D" (memory), // rdi = memory
                    "S" (max), // rsi = max runs
                    "d" (input), // rdx = input
                    "c" (output) // rcx = output
                : "rax", "memory"
            );
            // char al = (char)rax;
            // end = clock();
            return max - difference;
            // printf("here, looptime %ld %ld, %p, %d\n", prog->runtime, difference, input, strlen(input));
            // printf("ret value %d, char %c, hex 0x%02x\n", al, al, al);
            // printf("exectution time = %lf\n", (double)(end - begin) / CLOCKS_PER_SEC);

        }
};

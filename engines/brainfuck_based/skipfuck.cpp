#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include "brainfuck_base.hpp"

class Skipfuck : public Brainfuck_Base {
    private:
        // 1. rdi = pointer to memory space (aligned so as di will overflow back to the start
        // 2. rsi = max instructions to run; counts down to 0, then exits
        // 3. rdx = pointer to input (aligned so sil will overflow back to start)
        // 4. rcx = pointer to output (aligned so dl will overflow back to start)
        // 5. r8 = pointer to jump function
        // 6. r9 = pointer to input function
        // 7. r10 = pointer to output function
        // 8. r11 = second temporary register

        // RDI is the memory pointer
        constexpr static unsigned char rig[4] = { 0x66, 0xFF, 0xC7, 0x90 }; // inc di nop
        constexpr static unsigned char lef[4] = { 0x66, 0xFF, 0xCF, 0x90 }; // dec di nop
        constexpr static unsigned char inc[4] = { 0xFE, 0x07, 0x90, 0x90 }; // inc byte ptr[rdi] nop nop
        constexpr static unsigned char dec[4] = { 0xFE, 0x0F, 0x90, 0x90 }; // dec byte ptr[rdi] nop nop
        constexpr static unsigned char jmp[4] = { 0x90, 0x41, 0xFF, 0xD0 }; // call r8 nop
        constexpr static unsigned char com[4] = { 0x90, 0x41, 0xFF, 0xD1 }; // call r9 nop
        constexpr static unsigned char per[4] = { 0x90, 0x41, 0xFF, 0xD2 }; // call r10 nop
        constexpr static unsigned char jump_function[] = {
            0x58, // pop rax (get return address into rax)
            0x4C, 0x0F, 0xBE, 0x1F, // movsx r11, byte ptr [rdi]
            0x4A, 0x8D, 0x04, 0x98, // lea rax, [rax + r11*4] (jmp 0 should just continue as usual)
            0x48, 0xFF, 0xCE, // dec rsi (counts down from max)
            0x74, 0x02, // jz +2 (skip the jmp, fall into ret below)
            0xFF, 0xE0, // jmp rax
            0xC3, // ret (rsi hit 0: pop the outer return address and exit)
        };
        constexpr static unsigned char input_function[] = {
            0x8A, 0x02, // mov al, byte ptr[rdx] (move input into temporary register)
            0x88, 0x07, // mov byte ptr[rdi], al
            0xFE, 0xC2, // inc dl
            0xC3, // ret
        };
        constexpr static unsigned char output_function[] = {
            0x8A, 0x07, // mov al, byte ptr[rdi] (move current to temp)
            0x88, 0x01, // mov byte ptr[rcx], al (move temp to output)
            0xFE, 0xC1, // inc cl
            0xC3, // ret
        };
        void * jump_function_ptr;
        void * input_function_ptr;
        void * output_function_ptr;

        void append(char * str, int * str_size, const unsigned char * app, int app_size) {
            for(int i = 0; i < app_size; i++)
                str[(*str_size)++] = app[i];
        }

        char * program;

    public:
        Skipfuck() {
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
            sprintf(allowed_chars, "+-<>*,."); // add the new character to the allowed characters
            int index = 0;
            jump_function_ptr = program + index;
            append(program, &index, jump_function, sizeof(jump_function));
            input_function_ptr = program + index;
            append(program, &index, input_function, sizeof(input_function));
            output_function_ptr = program + index;
            append(program, &index, output_function, sizeof(output_function));
            for (int i = index; i < 508 + index; i++) { // pad beginning of program with rets in case brainfuck program jumps before the start (-128 * 4) + 4 first instruction = -508
                program[i] = 0xC3;
            }

            program += 508 + index; // move the program pointer to the end of the functions, but before the start fn
        }

        ~Skipfuck() {
            munmap(program, 4096 * 256);
        }

        void load(const void * code) {
            const std::string& code_ref = *reinterpret_cast<const std::string*>(code);
            // clock_t begin = clock();
            int index = 0;

            /*
            !!
            Way to make it faster: use char width enum type and then index array instead of switch case excep for ], maybe that's worth 0
            https://stackoverflow.com/questions/4879286/specifying-size-of-enum-type-in-c
            tried doing a 256 long array and directly accessing it and it wasn't any faster
            */
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
                    case '*':
                        append(program, &index, jmp, sizeof(jmp));
                        break;
                    default:
                        printf("invalid character found: %c\n", code_ref[x]);
                        exit(-1);
                }
            }

            for (int i = 0; i <= 508; i++) { // longer padding cause it can go past limits
                program[index++] = 0xC3; // this is ret
            }
        }

        size_t run(char input[256], char output[256], size_t max) {
            alignas(65536) unsigned char memory[65536] = {0};
            // clock_t end = clock();
            // printf("assemble time = %lf\n", (double)(end - begin) / CLOCKS_PER_SEC);
            // begin = clock();

            size_t difference;
            register void * r8v asm("r8") = jump_function_ptr;
            register void * r9v asm("r9") = input_function_ptr;
            register void * r10v asm("r10") = output_function_ptr;
            asm volatile(
                "call *%[fn]"
                : "=S" (difference) // rsi -> difference
                : [fn] "r" (program),
                    "D" (memory), // rdi = memory
                    "S" (max), // rsi = max runs
                    "d" (input), // rdx = input
                    "c" (output), // rcx = output
                    "r" (r8v), "r" (r9v), "r" (r10v)
                : "rax", "memory", "cc", "r11"
            );
            // char al = (char)rax;
            // end = clock();
            return max - difference;
            // printf("here, looptime %ld %ld, %p, %d\n", prog->runtime, difference, input, strlen(input));
            // printf("ret value %d, char %c, hex 0x%02x\n", al, al, al);
            // printf("exectution time = %lf\n", (double)(end - begin) / CLOCKS_PER_SEC);
        }
};

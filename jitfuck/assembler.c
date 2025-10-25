#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <evolver.h>

void append(char * str, int * str_size, const char * app, int app_size) {
    for(int i = 0; i < app_size; i++)
        str[(*str_size)++] = app[i];
}

// Takes three pointers and returns rax
typedef size_t (*fn)(char * memory, size_t runs, char * input, char * output);

char * program;

void init_jit() {
    program = mmap(NULL, // address
        4096 * 128, // page sizes are 4096, alloacte 256 so we always have space (max instruction (9 + 5)/2 = 7 * 65536 / 4096 = 112)
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

void free_jit() {
    munmap(program, 4096 * 128);
}

void jit(struct Program * prog, char input[256], char output[256]) {
    clock_t begin = clock();
    int index = 0;

    // 1. rdi = pointer to memory space (aligned so as di will overflow back to the start
    // 2. rsi = initial counter, should be 0 unless you want to start with some already
    // 2. rdx = pointer to input (aligned so sil will overflow back to start)
    // 3. rcx = pointer to output (aligned so dl will overflow back to start)

    // rcx - not passed, used as a temporary variable for io
    // rax - counter of instructions

    // RDI is the memory pointer
    char rig[] = {0x66, 0xFF, 0xC7}; // inc di
    char lef[] = {0x66, 0xFF, 0xCF}; // dec di
    char inc[] = {0xFE, 0x07}; // inc byte ptr[rdi]
    char dec[] = {0xFE, 0x0F}; // dec byte ptr[rdi]
    char com[] = {
        0x8A, 0x02, // mov al, byte ptr[rdx] (move input into temporary register)
        0x88, 0x07, // mov byte ptr[rdi], al
        0xFE, 0xC2, // inc dl
    };
    char per[] = {
        0x8A, 0x07, // mov al, byte ptr[rdi] (move current to temp)
        0x88, 0x01, // mov byte ptr[rcx], al (move temp to output)
        0xFE, 0xC1, // inc cl
    };
    char open[] = {
        0xE9, 0x00, 0x00, 0x00, 0x00, // jmp + offset, have to go fill the offset.
    };
    char close[] = {
        0xF6, 0x07, 0xFF, // test byte ptr [rdi], 0xFF
        0x0F, 0x85, 0x00, 0x00, 0x00, 0x00, // jnz + offset, have to fill the offset
    };

    /*
    !!
    Way to make it faster: use char width enum type and then index array instead of switch case excep for ], maybe that's worth 0
    https://stackoverflow.com/questions/4879286/specifying-size-of-enum-type-in-c
    tried doing a 256 long array and directly accessing it and it wasn't any faster
    */
    int vec[65536];
    int brackets = 0;
    for (unsigned short x = 0; x < prog->size; x++) {
        switch(prog->code[x]) {
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
                append(program, &index, open, sizeof(open));
                vec[brackets++] = index;
                break;
            case ']':
                if (brackets == 0)
                    return;

                append(program, &index, close, sizeof(close));
                int offset = vec[--brackets] - index;
                *(int *)(program + index - 4) = offset; // 4 less than the index after adding close
                int skip = -offset - sizeof(close); // for the forward jump, don't skip this part
                *(int *)(program + index + offset - 4) = skip;
                break;
        }
    }

    program[index++] = 0xC3; // this is ret

    if (brackets != 0)
        return;
    
    // for (int i = 0; i < index; i++) {
    //     if (i % 30 == 0)
    //         puts("");
    //     printf("0x%02x ", program[i] & 0xff);
    // }

    alignas(65536) unsigned char memory[65536] = {0};
    clock_t end = clock();
    printf("assemble time = %lf\n", (double)(end - begin) / CLOCKS_PER_SEC);
    begin = clock();

    fn jit_function = (fn)program;
    size_t rax = jit_function(memory, 0, input, output);
    // char al = (char)rax;
    end = clock();
    // printf("ret value %d, char %c, hex 0x%02x\n", al, al, al);
    printf("exectution time = %lf\n", (double)(end - begin) / CLOCKS_PER_SEC);

}

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>

void append(char * str, int * str_size, char * app, int app_size) {
    for(int i = 0; i < app_size; i++)
        str[(*str_size)++] = app[i];
}

// Takes three pointers and returns 1 byte (al)
typedef size_t (*fn)(char * memory, char * input, char * output);

char * program;
// Boilerplate code
char boilerplate [] = {
    0x48, 0x31, 0xC9, // xor rcx, rcx
    0x4D, 0x31, 0xC0, // xor r8, r8
    0x4D, 0x31, 0xC9, // xor r9, r9
    0x48, 0x31, 0xC0, // xor rax, rax (clean it for exit, rax should tell us last io)
};

void init_jit() {
    program = mmap(NULL, // address
        4096 * 256, // page sizes are 4096, alloacte 256 so we always have space (max instruction 10 * 65536 / 4096 = 160)
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1, // fd (not used here)
        0 // offset (not used here)
    );
    if (program == MAP_FAILED) {
        perror("failed to allocate memory");
        exit(1);
    }

    int index = 0;
    append(program, &index, boilerplate, sizeof(boilerplate));
}

void free_jit() {
    munmap(program, 4096 * 256);
}

char * jit(char * code, char input[256], char output[256]) {
    clock_t begin = clock();
    int index = sizeof(boilerplate); // always at the beginning never touched

    // 1. rdi = pointer to memory space
    // 2. rsi = pointer to input
    // 3. rdx = pointer to output

    // 4. rcx = index of memory
    // 5. r8  = index of input
    // 6. r9  = index of output

    // 0. al used as temporary storage of io
    // 7. r10 free for future use
    // 8. r11 free for future use

    // RDI is the memory pointer
    char rig[] = {0x66, 0xFF, 0xC1}; // inc cx
    char lef[] = {0x66, 0xFF, 0xC9}; // dec cx
    char inc[] = {0xFE, 0x04, 0x0F}; // inc byte ptr[rdi + rcx] (memory + memory offset)
    char dec[] = {0xFE, 0x0C, 0x0F}; // dec byte ptr[rdi + rcx] (memory + memory offset)
    char com[] = {
        0x42, 0x8A, 0x04, 0x06, // mov al, byte ptr[rsi + r8] (input + input offset)
        0x88, 0x44, 0x0F, // mov byte ptr[rdi + rcx], al
        0x41, 0xFE, 0xC0, // inc r8b (next char in input)
    };
    char per[] = {
        0x42, 0x8A, 0x04, 0x0A, // mov al, byte ptr[rdx + r9] (output + output offset)
        0x88, 0x04, 0x0F, // mov byte ptr[rdi + rcx], al
        0x41, 0xFE, 0xC1, // inc r9b (next char in output)
    };
    char open[] = {
        0xE9, 0x00, 0x00, 0x00, 0x00, // jmp + offset, have to go fill the offset. saves rip to stack 
    };
    char close[] = {
        0xF6, 0x04, 0x0F, 0xFF, // test byte ptr [rdi + rcx], 0xFF
        0x0F, 0x85, 0x00, 0x00, 0x00, 0x00, // jnz + offset, have to fill the offset
    };

    int brackets = 0;

    /*
    !!
    Way to make it faster: use char width enum type and then index array instead of switch case excep for ], maybe that's worth 0
    https://stackoverflow.com/questions/4879286/specifying-size-of-enum-type-in-c

    !! Use flat rdi, instead of offset, by using double memory then setting the low byte / word 0
    */ 
    for (unsigned short x = 0; x < strlen(code); x++) {
        switch(code[x]) {
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
                brackets++;
                break;
            case ']':
                brackets--;

                unsigned short temp = x;
                int offset = sizeof(open); // Once you get back to the original opening, it has to be after that
                int search = 0;
                do {
                    switch (code[temp--]) {
                        case 0: // will wrap around to get 0
                            return NULL; // close than open
                        case '[':
                            search++;
                            offset -= sizeof(open);
                            break;
                        case ']':
                            search--;
                            offset -= sizeof(close);
                            break;
                        case ',':
                        case '.':
                            offset -= sizeof(com);
                            break;
                        case '>':
                        case '<':
                        case '+':
                        case '-':
                            offset -= sizeof(rig);
                            break;
                    }
                } while (search);

                // Can safely assume we found it and temp is the number
                append(program, &index, close, sizeof(close));
                *(int *)(program + index - 4) = offset; // 4 less than the index after adding close

                int skip = offset + sizeof(close); // for the forward jump, don't skip this part
                skip *= -1; // forwards now;
                *(int *)(program + index + offset - 4) = skip;
                break;
        }
    }

    program[index++] = 0xC3; // this is ret

    if (brackets != 0)
        return NULL;
    
    // for (int i = 0; i < index; i++) {
    //     if (i % 30 == 0)
    //         puts("");
    //     printf("0x%02x ", program[i]);
    // }

    unsigned char memory[65536] = {0};
    clock_t end = clock();
    printf("assemble time = %lf\n", (double)(end - begin) / CLOCKS_PER_SEC);
    begin = clock();

    fn jit_function = (fn)program;
    size_t rax = jit_function(memory, input, output);
    char al = (char)rax;
    end = clock();
    printf("ret value %d, char %c, hex 0x%02x\n", al, al, al);
    printf("exectution time = %lf\n", (double)(end - begin) / CLOCKS_PER_SEC);

}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tt.h"

int main() {

    srand(time(NULL));

    FILE * f = fopen("bf", "r");

    fseek(f, 0L, SEEK_END);
    size_t len = ftell(f);
    
    fseek(f, 0L, SEEK_SET);	
    char * contents = malloc(len);	
    
    fread(contents, sizeof(char), len, f);
    fclose(f);

    game(contents);

    return 0;
}
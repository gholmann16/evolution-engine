#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {

    FILE * f = fopen("bf", "r");

    fseek(f, 0L, SEEK_END);
    len = ftell(f);
    
    fseek(f, 0L, SEEK_SET);	
    contents = malloc(len);	
    
    fread(contents, sizeof(char), len, f);
    fclose(f);

    game();


    return 0;
}
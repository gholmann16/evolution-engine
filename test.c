#include <stdio.h>
#include "brainfuck.h"

int main() {

    int x = run(">++++++++[+++>+<+[++++[+++++[<-+>+[[[++[[+++++++++[[+++++++++++++++[>-]<[+", NULL, 0);
    printf("%d\n", x);
}
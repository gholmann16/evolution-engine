#include <stdio.h>
#include <math.h>
int main(int argc, char * argv[]) {
    if (argc < 2) {
        puts("Must input a file");
        return -1;
    }

    FILE * file = fopen(argv[1], "r");
    unsigned char input, output;
    float multiplier;

    // uses graphviz to output https://dreampuf.github.io/GraphvizOnline/
    while (fscanf(file, "{%hhu, %hhu, %f},\n", &input, &output, &multiplier) == 3) {
        printf("%hhu -> %hhu [color=\"%s\", penwidth=%f, label=\"%f\"];\n", input, output, (multiplier > 0) ? "green" : "red", fabs(multiplier), multiplier);
    }

    fclose(file);
    return 0;
}

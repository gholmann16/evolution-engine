fast: brainfuck.o main.o generator.o
	gcc -flto main.c brainfuck.o generator.o

# game.o: game.c
# 	gcc -c -g game.c

main.o: main.c
	gcc -c -O3 -g main.c 

brainfuck.o: brainfuck.c
	gcc -c -O3 -g brainfuck.c

generator.o: generator.c
	gcc -c -O3 -g generator.c

clean:
	rm *.o
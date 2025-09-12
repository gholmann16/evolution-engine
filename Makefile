a.out: brainfuck.o main.c generator.o
	gcc -g main.c brainfuck.o generator.o

# game.o: game.c
# 	gcc -c -g game.c

brainfuck.o: brainfuck.c
	gcc -c -g brainfuck.c

generator.o: generator.c
	gcc -c -g generator.c

clean:
	rm *.o
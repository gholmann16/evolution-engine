risc: sucrisc.o main.o compiler.o vector.o
	gcc -flto sucrisc.o main.o compiler.o vector.o

compiler.o: compiler.c
	gcc -c -O3 -g compiler.c

vector.o: vector.c
	gcc -c -O3 -g vector.c

sucrisc.o: sucrisc.c
	gcc -c -O3 -g sucrisc.c

fast: brainfuck.o main.o generator.o
	gcc -flto main.o brainfuck.o generator.o

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
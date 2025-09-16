CFLAGS := -Wall -c -O3 -g -I.
LDLIBS := -flto

fast: main.o brainfuck.o generator.o
	gcc $(LDLIBS) main.o brainfuck.o generator.o -o fast

risc: sucrisc.o main.o compiler.o vector.o
	gcc $(LDLIBS) sucrisc.o main.o compiler.o vector.o -o risc

compiler: standalone.o compiler.o vector.o
	gcc $(LDLIBS) standalone.o compiler.o vector.o -o compiler

# Objects

main.o: main.c
	gcc $(CFLAGS) main.c 

vector.o: sucrisc/vector.c
	gcc $(CFLAGS) sucrisc/vector.c

standalone.o: sucrisc/standalone.c
	gcc $(CFLAGS) sucrisc/standalone.c

compiler.o: sucrisc/compiler.c
	gcc $(CFLAGS) sucrisc/compiler.c

sucrisc.o: sucrisc/sucrisc.c
	gcc $(CFLAGS) sucrisc/sucrisc.c

brainfuck.o: brainfuck/brainfuck.c
	gcc $(CFLAGS) brainfuck/brainfuck.c

generator.o: brainfuck/generator.c
	gcc $(CFLAGS) brainfuck/generator.c

# game.o: game.c
# 	gcc -c -g game.c

clean:
	rm *.o
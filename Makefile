CFLAGS := -Wall -c -O3 -g -I. -Wno-deprecated-declarations
LDLIBS := -flto

fast: cli.o evolver.o brainfuck.o generator.o crc8.o
	gcc $(LDLIBS) cli.o evolver.o brainfuck.o generator.o crc8.o -o fast

gui: CFLAGS += `pkg-config --cflags libadwaita-1`
gui: LDLIBS += `pkg-config --libs libadwaita-1`
gui: main.o evolver.o brainfuck.o generator.o
	gcc $(LDLIBS) main.o evolver.o brainfuck.o generator.o -o gui

risc: sucrisc.o main.o compiler.o vector.o
	gcc $(LDLIBS) sucrisc.o main.o compiler.o vector.o -o risc

compiler: standalone.o compiler.o vector.o
	gcc $(LDLIBS) standalone.o compiler.o vector.o -o compiler

# Objects

crc8.o: crc8.c
	gcc $(CFLAGS) crc8.c

main.o: main.c
	gcc $(CFLAGS) main.c 

evolver.o: evolver.c
	gcc $(CFLAGS) evolver.c

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
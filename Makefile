CFLAGS := -Wall -c -O3 -g -I. -Wno-deprecated-declarations
LDLIBS := -flto

jit_different_tests.out: cli.o evolver.o assembler.o generator.o runner.o output.o tictactoe.o crc8.o
	g++ $(LDLIBS) cli.o crc8.o evolver.o assembler.o generator.o runner.o output.o tictactoe.o -o jit_different_tests.out

output.out: cli.o evolver.o brainfuck.o generator.o runner.o output.o
	gcc $(LDLIBS) cli.o evolver.o brainfuck.o generator.o runner.o output.o -o output.out

crc8.out: cli.o evolver.o brainfuck.o generator.o runner.o crc8.o
	gcc $(LDLIBS) cli.o evolver.o brainfuck.o generator.o runner.o crc8.o -o crc8.out

jit_output.out: cli.o evolver.o assembler.o generator.o runner.o output.o
	gcc $(LDLIBS) cli.o evolver.o assembler.o generator.o runner.o output.o -o jit_output.out

jit_crc8.out: cli.o evolver.o assembler.o generator.o runner.o crc8.o
	gcc $(LDLIBS) cli.o evolver.o assembler.o generator.o runner.o crc8.o -o jit_crc8.out

gui: CFLAGS += `pkg-config --cflags libadwaita-1`
gui: LDLIBS += `pkg-config --libs libadwaita-1`
gui: main.o evolver.o brainfuck.o generator.o
	gcc $(LDLIBS) main.o evolver.o brainfuck.o generator.o -o gui

risc: sucrisc.o main.o compiler.o vector.o
	gcc $(LDLIBS) sucrisc.o main.o compiler.o vector.o -o risc

compiler: standalone.o compiler.o vector.o
	gcc $(LDLIBS) standalone.o compiler.o vector.o -o compiler

# Objects

tictactoe.o: tests/tictactoe.cpp
	gcc $(CFLAGS) tests/tictactoe.cpp

assembler.o: jitfuck/assembler.c
	gcc $(CFLAGS) jitfuck/assembler.c

output.o: tests/output.cpp
	g++ $(CFLAGS) tests/output.cpp

crc8.o: tests/crc8.cpp
	g++ $(CFLAGS) tests/crc8.cpp

runner.o: runner.c
	gcc $(CFLAGS) runner.c

main.o: main.c
	gcc $(CFLAGS) main.c 

evolver.o: evolver.cpp
	g++ $(CFLAGS) evolver.cpp

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
	rm *.out

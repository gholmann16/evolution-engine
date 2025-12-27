CFLAGS := -Wall -c -O3 -g -Wno-deprecated-declarations -std=c++20 -march=native -flto
LDLIBS := -flto

monolith.out: cli.o runner.o state.o assembler.o brainfuck.o output.o tictactoe.o crc8.o tic_off.o network.o
	g++ $(LDLIBS) cli.o runner.o state.o assembler.o brainfuck.o output.o tictactoe.o crc8.o network.o tic_off.o -o monolith.out

gui: CFLAGS += `pkg-config --cflags libadwaita-1`
gui: LDLIBS += `pkg-config --libs libadwaita-1`
gui: main.o evolver.o brainfuck.o generator.o
	gcc $(LDLIBS) main.o evolver.o brainfuck.o generator.o -o gui

risc: sucrisc.o main.o compiler.o vector.o
	gcc $(LDLIBS) sucrisc.o main.o compiler.o vector.o -o risc

compiler: standalone.o compiler.o vector.o
	gcc $(LDLIBS) standalone.o compiler.o vector.o -o compiler

# Objects

tic_off.o: tests/tic_off.cpp
	g++ $(CFLAGS) tests/tic_off.cpp

network.o: engines/neural_based/network.cpp
	g++ $(CFLAGS) engines/neural_based/network.cpp

cli.o: cli.cpp
	g++ $(CFLAGS) cli.cpp

tictactoe.o: tests/tictactoe.cpp
	g++ $(CFLAGS) tests/tictactoe.cpp

assembler.o: engines/brainfuck_based/assembler.cpp
	g++ $(CFLAGS) engines/brainfuck_based/assembler.cpp

output.o: tests/output.cpp
	g++ $(CFLAGS) tests/output.cpp

crc8.o: tests/crc8.cpp
	g++ $(CFLAGS) tests/crc8.cpp

runner.o: runner.cpp
	g++ $(CFLAGS) runner.cpp

main.o: main.c
	gcc $(CFLAGS) main.c 

state.o: state.cpp
	g++ $(CFLAGS) state.cpp

vector.o: sucrisc/vector.c
	gcc $(CFLAGS) sucrisc/vector.c

standalone.o: sucrisc/standalone.c
	gcc $(CFLAGS) sucrisc/standalone.c

compiler.o: sucrisc/compiler.c
	gcc $(CFLAGS) sucrisc/compiler.c

sucrisc.o: sucrisc/sucrisc.c
	gcc $(CFLAGS) sucrisc/sucrisc.c

brainfuck.o: engines/brainfuck_based/brainfuck.cpp
	g++ $(CFLAGS) engines/brainfuck_based/brainfuck.cpp

clean:
	rm *.o
	rm *.out

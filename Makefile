CFLAGS := -Wall -c -O3 -g -Wno-deprecated-declarations -std=c++20 -march=native -flto -I.
LDLIBS := -flto -g

monolith.out: cli.o runner.o engines.o tests.o evolvers.o
	g++ $(LDLIBS) cli.o runner.o engines.o tests.o evolvers.o -o monolith.out

gui: CFLAGS += `pkg-config --cflags libadwaita-1`
gui: LDLIBS += `pkg-config --libs libadwaita-1`
gui: main.o evolver.o brainfuck.o generator.o
	gcc $(LDLIBS) main.o evolver.o brainfuck.o generator.o -o gui

# Objects

tests.o: tests/tests.cpp tests/crc8.cpp tests/output.cpp tests/tic_off.cpp tests/tictactoe.cpp
	g++ $(CFLAGS) tests/tests.cpp

evolvers.o: evolvers/evolvers.cpp evolvers/above_average.cpp evolvers/squarelite.cpp
	g++ $(CFLAGS) evolvers/evolvers.cpp

engines.o: engines/engines.cpp engines/brainfuck_based/assembler.cpp engines/brainfuck_based/brainfuck.cpp engines/brainfuck_based/brainfuck_base.hpp engines/neural_based/network.cpp engines/neural_based/brain.hpp
	g++ $(CFLAGS) engines/engines.cpp

cli.o: cli.cpp
	g++ $(CFLAGS) cli.cpp

runner.o: runner.cpp
	g++ $(CFLAGS) runner.cpp

main.o: main.c
	gcc $(CFLAGS) main.c 

clean:
	rm *.o
	rm *.out

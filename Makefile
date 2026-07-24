CFLAGS := -Wall -c -O3 -g -Wno-deprecated-declarations -std=c++20 -march=native -flto -ftls-model=initial-exec -I. -pthread
LDLIBS := -flto -g -pthread

monolith.out: cli.o runner.o engines.o tests.o evolvers.o
	g++ $(LDLIBS) cli.o runner.o engines.o tests.o evolvers.o -o monolith.out

gui.out: gui.o engines.o tests.o evolvers.o
	g++ $(LDLIBS) `pkg-config --libs gtk4 libadwaita-1` gui.o engines.o tests.o evolvers.o -o gui.out

# Objects

tests.o: state.hpp tests/tests.cpp tests/crc8.cpp tests/output.cpp tests/tic_off.cpp tests/tictactoe.cpp tests/tournament_toe.cpp tests/tictactoe_common.hpp tests/add.cpp
	g++ $(CFLAGS) tests/tests.cpp

evolvers.o: state.hpp evolvers/evolvers.cpp evolvers/standard.hpp evolvers/above_average.cpp evolvers/squarelite.cpp evolvers/top_10_percent.cpp
	g++ $(CFLAGS) evolvers/evolvers.cpp

engines.o: state.hpp engines/engines.cpp engines/brainfuck_based/assembler.cpp engines/brainfuck_based/brainfuck.cpp engines/brainfuck_based/brainfuck_base.hpp engines/brainfuck_based/skipfuck.cpp engines/neural_based/network.cpp engines/neural_based/racehorse.cpp engines/neural_based/brain.hpp
	g++ $(CFLAGS) engines/engines.cpp

cli.o: state.hpp cli.cpp
	g++ $(CFLAGS) cli.cpp

runner.o: state.hpp runner.cpp
	g++ $(CFLAGS) runner.cpp

gui.o: CFLAGS += `pkg-config --cflags gtk4 libadwaita-1`
gui.o: state.hpp engines/neural_based/brain.hpp gui.cpp
	g++ $(CFLAGS) gui.cpp

clean:
	rm *.o
	rm *.out

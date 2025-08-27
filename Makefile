a.out: turn.o game.o main.c generator.o
	gcc -g main.c game.o turn.o generator.o

game.o: game.c
	gcc -c game.c

turn.o: turn.c
	gcc -c turn.c

generator.o: generator.c
	gcc -c generator.c

clean:
	rm *.o
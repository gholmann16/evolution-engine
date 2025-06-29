a.out: turn.o game.o main.c
	gcc main.c game.o turn.o

game.o: game.c
	gcc -c game.c

turn.o: turn.c
	gcc -c turn.c

clean:
	rm *.o
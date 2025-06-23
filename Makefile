a.out: turn.o tt.o main.c
	gcc main.c tt.o turn.o

tt.o: tt.c
	gcc -c tt.c

turn.o: turn.c
	gcc -c turn.c
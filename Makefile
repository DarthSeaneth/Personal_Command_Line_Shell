CC = gcc
CFLAGS = -std=c99 -g -Wall -fsanitize=address,undefined

all: mysh test test2

mysh: mysh.o arraylist.o
	$(CC) $(CFLAGS) $^ -o $@

mysh.o arraylist.o: arraylist.h

arraylist-dev.o: arraylist.c arraylist.h
	$(CC) $(CFLAGS) -DSAFE -DDEBUG=2 $< -o $@

test: TestProgram.c
	$(CC) $(CFLAGS) $^ -o $@

test2: TestProgram2.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf *.o mysh test test2
CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=c99

editor: main.o editor.o
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c editor.h
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean
clean:
	rm -rf *.o

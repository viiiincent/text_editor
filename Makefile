CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=c99
OBJECTS=main.o editor.o syntax_highlight.o abuff.o
HEADERS=editor.h syntax_highlight.h abuff.h
INCLUDES := -I.

editor: $(OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean
clean:
	rm -rf *.o

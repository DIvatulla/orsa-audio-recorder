CC = gcc
CFLAGS = -g -Wall -Wextra -Iinclude

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))

main: $(OBJ)
	$(CC) $(OBJ) -o $@ -lncurses

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm build/*.o main

.PHONY:	clean

CC = gcc
CFLAGS = -g -Wall -Wextra -Iinclude

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
LIBS := -lasound -lm -lpthread

main: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LIBS)

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS)

.PHONY:	clean

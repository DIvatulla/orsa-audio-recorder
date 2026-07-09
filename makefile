CC = gcc
CFLAGS = -g -Wall -Wextra -Iinclude
MDEF =

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
LIBS := -lasound -lm -lpthread -lmp3lame -lmpg123

main: $(OBJ)
	$(CC) $(CFLAGS) $(MDEF) $(OBJ) -o $@ $(LIBS)

build/%.o: src/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS)

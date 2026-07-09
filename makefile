CC = gcc
CFLAGS = -g -Wall -Wextra -Iinclude

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
LIBS := -lasound -lm -lpthread -lmp3lame -lmpg123

main: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LIBS)

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS)

.PHONY:	clean
clean:
	rm -f $(OBJ)
